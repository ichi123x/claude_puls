// ClaudePulseDlg.cpp : メインダイアログの実装
//

#include "pch.h"
#include "ClaudePulse.h"
#include "ClaudePulseDlg.h"

#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	// 配色（PulseMeterCtrl と同系のダークテーマ）
	const COLORREF kColorBack = RGB(11, 18, 32);    // ダイアログ背景
	const COLORREF kColorLabel = RGB(140, 155, 180); // 項目名
	const COLORREF kColorValue = RGB(230, 235, 245); // 数値
	const COLORREF kColorModel = RGB(64, 224, 144);  // モデル名（緑）
	const COLORREF kColorFile = RGB(100, 115, 140);  // 監視ファイルパス・補足情報

	const UINT_PTR kTimerIdPoll = 1;        // 使用量ポーリングタイマー
	const UINT     kPollIntervalMs = 1000;   // JSONL 監視の更新間隔（ミリ秒）
	const UINT_PTR kTimerIdUsage = 2;       // プラン使用制限の取得タイマー
	const UINT     kUsageIntervalMs = 60000; // 使用制限の取得間隔（ミリ秒）

	// 表示サイズ「小」の縮小率。中の面積比1/4＝縦横それぞれ1/2。
	// 縦横比で1/4にしたい場合はここを 0.25 にする
	const double kSmallScale = 0.5;

	// フォントのポイント数の1/10表記（CreatePointFont 用）
	const int kUiFontDeciPt = 90;     // 9pt（ダイアログ既定と同じ）
	const int kModelFontDeciPt = 160; // 16pt（モデル名）

	// 曜日表示（CTime::GetDayOfWeek は 1=日曜）
	const LPCWSTR kWeekdays[] = { L"", L"日", L"月", L"火", L"水", L"木", L"金", L"土" };

	// 詳細表示オフで隠すコントロール（トークン集計と「プラン使用制限」見出し行）。
	// 使用制限のバー3本（現在のセッション・週間制限×2）はオフでも表示し続ける
	bool IsDetailControl(int id)
	{
		static const int kDetailIds[] = {
			IDC_STATIC_INPUT_LABEL, IDC_STATIC_INPUT,
			IDC_STATIC_OUTPUT_LABEL, IDC_STATIC_OUTPUT,
			IDC_STATIC_CACHE_R_LABEL, IDC_STATIC_CACHE_R,
			IDC_STATIC_CACHE_C_LABEL, IDC_STATIC_CACHE_C,
			IDC_STATIC_TOTAL_LABEL, IDC_STATIC_TOTAL,
			IDC_STATIC_RATE_LABEL, IDC_STATIC_RATE,
			IDC_STATIC_LIMIT_HEADER, IDC_STATIC_LIMIT_STATUS,
		};
		for (int detailId : kDetailIds)
		{
			if (id == detailId)
			{
				return true;
			}
		}
		return false;
	}
}

// CClaudePulseDlg ダイアログ

CClaudePulseDlg::CClaudePulseDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CLAUDEPULSE_DIALOG, pParent)
	, m_pDialogFont(nullptr)
	, m_baseClient(0, 0)
	, m_sizeSmall(false)
	, m_detailOn(true)
	, m_timerId(0)
	, m_usageTimerId(0)
	, m_usageFetchInProgress(false)
{
}

void CClaudePulseDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PULSE_METER, m_pulseMeter);
	DDX_Control(pDX, IDC_BAR_SESSION, m_barSession);
	DDX_Control(pDX, IDC_BAR_WEEK_ALL, m_barWeekAll);
	DDX_Control(pDX, IDC_BAR_WEEK_MODEL, m_barWeekModel);
	DDX_Control(pDX, IDC_RADIO_TOPMOST, m_radioTopMost);
	DDX_Control(pDX, IDC_RADIO_NORMAL, m_radioNormal);
	DDX_Control(pDX, IDC_RADIO_SIZE_SMALL, m_radioSizeSmall);
	DDX_Control(pDX, IDC_RADIO_SIZE_MEDIUM, m_radioSizeMedium);
	DDX_Control(pDX, IDC_RADIO_DETAIL_OFF, m_radioDetailOff);
	DDX_Control(pDX, IDC_RADIO_DETAIL_ON, m_radioDetailOn);
}

BEGIN_MESSAGE_MAP(CClaudePulseDlg, CDialogEx)
	ON_WM_TIMER()
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_APP_USAGE_UPDATED, &CClaudePulseDlg::OnUsageUpdated)
	ON_BN_CLICKED(IDC_RADIO_TOPMOST, &CClaudePulseDlg::OnClickedRadioTopMost)
	ON_BN_CLICKED(IDC_RADIO_NORMAL, &CClaudePulseDlg::OnClickedRadioNormal)
	ON_BN_CLICKED(IDC_RADIO_SIZE_SMALL, &CClaudePulseDlg::OnClickedRadioSizeSmall)
	ON_BN_CLICKED(IDC_RADIO_SIZE_MEDIUM, &CClaudePulseDlg::OnClickedRadioSizeMedium)
	ON_BN_CLICKED(IDC_RADIO_DETAIL_OFF, &CClaudePulseDlg::OnClickedRadioDetailOff)
	ON_BN_CLICKED(IDC_RADIO_DETAIL_ON, &CClaudePulseDlg::OnClickedRadioDetailOn)
END_MESSAGE_MAP()

// CClaudePulseDlg メッセージ ハンドラー

BOOL CClaudePulseDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ダークテーマの背景ブラシ
	m_backBrush.CreateSolidBrush(kColorBack);

	// モデル名用の大きめフォント（16pt / Segoe UI）と、表示サイズ「小」用の縮小フォント
	m_modelFont.CreatePointFont(kModelFontDeciPt, L"Segoe UI");
	m_modelFontSmall.CreatePointFont(static_cast<int>(kModelFontDeciPt * kSmallScale + 0.5), L"Segoe UI");
	m_uiFontSmall.CreatePointFont(static_cast<int>(kUiFontDeciPt * kSmallScale + 0.5), L"Segoe UI");
	m_pDialogFont = GetFont();
	GetDlgItem(IDC_STATIC_MODEL)->SetFont(&m_modelFont);

	// 日本語UI文字列はコードから設定する（ADR-005：.rc は ASCII のみ）
	SetWindowText(L"ClaudePulse — Claude Code 使用量モニター");

	// セクション見出し（グループボックス）。ビジュアルスタイル有効時は見出しの
	// 文字色が WM_CTLCOLORSTATIC を無視してテーマ既定色（暗色）で描かれ、
	// ダーク背景で読めなくなるため、クラシック描画へ切り替えて配色を効かせる
	SetDlgItemText(IDC_GROUP_WINDOW, L"ウィンドウ表示プライオリティ");
	SetDlgItemText(IDC_GROUP_SIZE, L"アプリ表示サイズ");
	SetDlgItemText(IDC_GROUP_DETAIL, L"詳細表示");
	SetDlgItemText(IDC_GROUP_MONITOR, L"使用状況");
	::SetWindowTheme(GetDlgItem(IDC_GROUP_WINDOW)->GetSafeHwnd(), L" ", L" ");
	::SetWindowTheme(GetDlgItem(IDC_GROUP_SIZE)->GetSafeHwnd(), L" ", L" ");
	::SetWindowTheme(GetDlgItem(IDC_GROUP_DETAIL)->GetSafeHwnd(), L" ", L" ");
	::SetWindowTheme(GetDlgItem(IDC_GROUP_MONITOR)->GetSafeHwnd(), L" ", L" ");

	SetDlgItemText(IDC_STATIC_MODEL, L"モデル検出待ち…");
	SetDlgItemText(IDC_STATIC_INPUT_LABEL, L"入力トークン：");
	SetDlgItemText(IDC_STATIC_OUTPUT_LABEL, L"出力トークン：");
	SetDlgItemText(IDC_STATIC_CACHE_R_LABEL, L"キャッシュ読込：");
	SetDlgItemText(IDC_STATIC_CACHE_C_LABEL, L"キャッシュ作成：");
	SetDlgItemText(IDC_STATIC_TOTAL_LABEL, L"合計トークン：");
	SetDlgItemText(IDC_STATIC_RATE_LABEL, L"消費レート：");

	// プラン使用制限セクション（Claude Desktop の設定画面と同形式）
	SetDlgItemText(IDC_STATIC_LIMIT_HEADER, L"プラン使用制限");
	SetDlgItemText(IDC_STATIC_LIMIT_STATUS, L"取得中…");
	SetDlgItemText(IDC_STATIC_SESSION_LABEL, L"現在のセッション");
	SetDlgItemText(IDC_STATIC_WEEK_ALL_LABEL, L"週間制限（すべてのモデル）");
	SetDlgItemText(IDC_STATIC_WEEK_MODEL_LABEL, L"週間制限（モデル別）");
	SetDlgItemText(IDC_STATIC_SESSION_PCT, L"—");
	SetDlgItemText(IDC_STATIC_WEEK_ALL_PCT, L"—");
	SetDlgItemText(IDC_STATIC_WEEK_MODEL_PCT, L"—");

	// ウィンドウの最前面表示切り替え（ADR-007）。
	// 前回終了時の選択を復元する。未保存（初回起動）の既定値は「最前面」
	SetDlgItemText(IDC_RADIO_TOPMOST, L"最前面");
	SetDlgItemText(IDC_RADIO_NORMAL, L"通常");

	bool topMost = theApp.GetProfileInt(L"Settings", L"AlwaysOnTop", 1) != 0;
	m_radioTopMost.SetChecked(topMost);
	m_radioNormal.SetChecked(!topMost);
	ApplyAlwaysOnTop(topMost);

	// アプリ表示サイズ（小＝中の面積比1/4）と詳細表示（オフ＝トークン集計と
	// プラン使用制限を畳む）。どちらも前回終了時の選択を復元する。
	// 未保存（初回起動）の既定値は「中」「オン」。
	// 基準レイアウトの記録は、レイアウト変更が一度も走っていないこの時点で行う
	SetDlgItemText(IDC_RADIO_SIZE_SMALL, L"小");
	SetDlgItemText(IDC_RADIO_SIZE_MEDIUM, L"中");
	SetDlgItemText(IDC_RADIO_DETAIL_OFF, L"オフ");
	SetDlgItemText(IDC_RADIO_DETAIL_ON, L"オン");
	CaptureBaseLayout();

	m_sizeSmall = theApp.GetProfileInt(L"Settings", L"DisplaySize", 1) == 0;
	m_radioSizeSmall.SetChecked(m_sizeSmall);
	m_radioSizeMedium.SetChecked(!m_sizeSmall);

	m_detailOn = theApp.GetProfileInt(L"Settings", L"DetailDisplay", 1) != 0;
	m_radioDetailOff.SetChecked(!m_detailOn);
	m_radioDetailOn.SetChecked(m_detailOn);

	UpdateLayout();

	// 監視の初期化。projects フォルダが無くてもエラーにせず待機表示とする
	if (m_monitor.Initialize())
	{
		SetDlgItemText(IDC_STATIC_FILE, L"監視中…");
	}
	else
	{
		SetDlgItemText(IDC_STATIC_FILE,
			L"%USERPROFILE%\\.claude\\projects が見つかりません（Claude Code の実行を待機中）");
	}

	// 初回は即時反映し、以降は1秒間隔で更新する。
	// 注意：OnTimer() を直接呼んではいけない。WM_INITDIALOG の処理中に
	// CWnd::Default() が呼ばれて OnInitDialog が再入し、GDIオブジェクトの
	// 二重生成アサーション（wingdi.cpp）になるため、共通処理のみを呼ぶ
	PollAndRefresh();
	m_timerId = SetTimer(kTimerIdPoll, kPollIntervalMs, nullptr);

	// プラン使用制限は初回すぐ取得し、以降は60秒間隔で更新する
	StartUsageFetch();
	m_usageTimerId = SetTimer(kTimerIdUsage, kUsageIntervalMs, nullptr);

	return TRUE;  // フォーカスをコントロールに設定した場合を除き、TRUE を返す
}

void CClaudePulseDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == kTimerIdPoll)
	{
		PollAndRefresh();
	}
	else if (nIDEvent == kTimerIdUsage)
	{
		StartUsageFetch();
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CClaudePulseDlg::PollAndRefresh()
{
	// projects フォルダが後から作られた場合に備えて再解決する
	m_monitor.Initialize();

	UsageStats stats;
	m_monitor.Poll(stats);

	UpdateDisplay(stats);
	m_pulseMeter.AddSample(stats.deltaTokens, stats.valid);
}

void CClaudePulseDlg::UpdateDisplay(const UsageStats& stats)
{
	if (stats.valid)
	{
		// 三項演算子の両辺を LPCWSTR に揃える（CString と文字列リテラルの混在は型があいまいになる）
		SetDlgItemText(IDC_STATIC_MODEL,
			stats.modelName.IsEmpty() ? L"（モデル未検出）" : static_cast<LPCWSTR>(stats.modelName));

		SetDlgItemText(IDC_STATIC_INPUT, FormatWithCommas(stats.inputTokens));
		SetDlgItemText(IDC_STATIC_OUTPUT, FormatWithCommas(stats.outputTokens));
		SetDlgItemText(IDC_STATIC_CACHE_R, FormatWithCommas(stats.cacheReadTokens));
		SetDlgItemText(IDC_STATIC_CACHE_C, FormatWithCommas(stats.cacheCreationTokens));
		SetDlgItemText(IDC_STATIC_TOTAL, FormatWithCommas(stats.Total()));

		CString rate;
		rate.Format(L"%s tok/s", (LPCWSTR)FormatWithCommas(stats.deltaTokens));
		SetDlgItemText(IDC_STATIC_RATE, rate);

		CString watch;
		watch.Format(L"監視中：%s", (LPCWSTR)stats.watchedFile);
		SetDlgItemText(IDC_STATIC_FILE, watch);
	}
	else
	{
		SetDlgItemText(IDC_STATIC_MODEL, L"モデル検出待ち…");
		SetDlgItemText(IDC_STATIC_RATE, L"0 tok/s");
		SetDlgItemText(IDC_STATIC_FILE,
			L"セッション記録（*.jsonl）を待機中 — Claude Code を実行してください");
	}
}

void CClaudePulseDlg::StartUsageFetch()
{
	// 前回の取得が終わっていなければスキップ（多重起動防止）
	if (m_usageFetchInProgress)
	{
		return;
	}
	m_usageFetchInProgress = true;

	AfxBeginThread(UsageFetchThread, reinterpret_cast<LPVOID>(GetSafeHwnd()));
}

UINT AFX_CDECL CClaudePulseDlg::UsageFetchThread(LPVOID pParam)
{
	HWND hwnd = reinterpret_cast<HWND>(pParam);

	// 結果はヒープに確保して PostMessage で渡し、受信側（OnUsageUpdated）で解放する
	auto* pResult = new UsageLimitsResult();
	CUsageLimitsClient::Fetch(*pResult);

	if (!::IsWindow(hwnd) ||
	    !::PostMessage(hwnd, WM_APP_USAGE_UPDATED, 0, reinterpret_cast<LPARAM>(pResult)))
	{
		// ウィンドウが閉じられていた場合はここで解放する
		delete pResult;
	}
	return 0;
}

LRESULT CClaudePulseDlg::OnUsageUpdated(WPARAM /*wParam*/, LPARAM lParam)
{
	std::unique_ptr<UsageLimitsResult> pResult(
		reinterpret_cast<UsageLimitsResult*>(lParam));

	m_usageFetchInProgress = false;
	ApplyUsageLimits(*pResult);
	return 0;
}

void CClaudePulseDlg::ApplyUsageLimits(const UsageLimitsResult& result)
{
	// ヘッダー（プラン名が取れたら「プラン使用制限（Pro）」のように表示）
	CString header = L"プラン使用制限";
	if (!result.planName.IsEmpty())
	{
		header.AppendFormat(L"（%s）", (LPCWSTR)result.planName);
	}
	SetDlgItemText(IDC_STATIC_LIMIT_HEADER, header);

	if (!result.ok)
	{
		// 取得失敗時はステータスのみ更新し、前回の表示を保持する
		SetDlgItemText(IDC_STATIC_LIMIT_STATUS, result.errorMessage);
		return;
	}

	CString status;
	status.Format(L"最終更新 %s", (LPCWSTR)result.fetchedAt.Format(L"%H:%M:%S"));
	SetDlgItemText(IDC_STATIC_LIMIT_STATUS, status);

	UpdateLimitRow(result.fiveHour, L"現在のセッション", true,
		IDC_STATIC_SESSION_LABEL, m_barSession, IDC_STATIC_SESSION_PCT);

	UpdateLimitRow(result.sevenDay, L"週間制限（すべてのモデル）", false,
		IDC_STATIC_WEEK_ALL_LABEL, m_barWeekAll, IDC_STATIC_WEEK_ALL_PCT);

	CString modelLabel;
	modelLabel.Format(L"週間制限（%s）",
		result.sevenDayModelLabel.IsEmpty() ? L"モデル別"
		                                    : (LPCWSTR)result.sevenDayModelLabel);
	UpdateLimitRow(result.sevenDayModel, modelLabel, false,
		IDC_STATIC_WEEK_MODEL_LABEL, m_barWeekModel, IDC_STATIC_WEEK_MODEL_PCT);
}

void CClaudePulseDlg::UpdateLimitRow(const UsageSection& section, const CString& baseLabel,
                                     bool relativeReset, int labelId, CUsageBarCtrl& bar, int pctId)
{
	if (!section.present)
	{
		SetDlgItemText(labelId, baseLabel);
		bar.SetPercent(-1.0);
		SetDlgItemText(pctId, L"—");
		return;
	}

	// ラベルにリセット時期を添える（Desktop と同様、セッションは相対・週間は絶対時刻）
	CString label = baseLabel;
	if (section.hasReset)
	{
		if (relativeReset)
		{
			CTimeSpan span = section.resetTime - CTime::GetCurrentTime();
			if (span.GetTotalSeconds() <= 0)
			{
				label += L"・まもなくリセット";
			}
			else if (span.GetTotalHours() > 0)
			{
				label.AppendFormat(L"・%lld時間%d分後にリセット",
					static_cast<LONGLONG>(span.GetTotalHours()), span.GetMinutes());
			}
			else
			{
				label.AppendFormat(L"・%d分後にリセット", span.GetMinutes());
			}
		}
		else
		{
			label.AppendFormat(L"・%s（%s）にリセット",
				(LPCWSTR)section.resetTime.Format(L"%H:%M"),
				kWeekdays[section.resetTime.GetDayOfWeek()]);
		}
	}
	SetDlgItemText(labelId, label);

	bar.SetPercent(section.utilization);

	CString pct;
	pct.Format(L"%d%% 使用済み", static_cast<int>(section.utilization + 0.5));
	SetDlgItemText(pctId, pct);
}

CString CClaudePulseDlg::FormatWithCommas(ULONGLONG value)
{
	CString digits;
	digits.Format(L"%llu", value);

	// 右から3桁ごとにカンマを挿入する
	CString result;
	int count = 0;
	for (int i = digits.GetLength() - 1; i >= 0; --i)
	{
		result.Insert(0, digits[i]);
		if (++count == 3 && i > 0)
		{
			result.Insert(0, L',');
			count = 0;
		}
	}
	return result;
}

HBRUSH CClaudePulseDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

	// ダイアログ全体と静的コントロールをダークテーマにする
	if (nCtlColor == CTLCOLOR_DLG)
	{
		return m_backBrush;
	}
	if (nCtlColor == CTLCOLOR_STATIC)
	{
		pDC->SetBkMode(TRANSPARENT);

		switch (pWnd->GetDlgCtrlID())
		{
		case IDC_STATIC_MODEL:
			pDC->SetTextColor(kColorModel);
			break;
		case IDC_STATIC_FILE:
		case IDC_STATIC_LIMIT_STATUS:
			pDC->SetTextColor(kColorFile);
			break;
		case IDC_STATIC_INPUT:
		case IDC_STATIC_OUTPUT:
		case IDC_STATIC_CACHE_R:
		case IDC_STATIC_CACHE_C:
		case IDC_STATIC_TOTAL:
		case IDC_STATIC_RATE:
		case IDC_STATIC_SESSION_PCT:
		case IDC_STATIC_WEEK_ALL_PCT:
		case IDC_STATIC_WEEK_MODEL_PCT:
			pDC->SetTextColor(kColorValue);
			break;
		default:
			pDC->SetTextColor(kColorLabel);
			break;
		}
		return m_backBrush;
	}

	return hbr;
}

void CClaudePulseDlg::OnClickedRadioTopMost()
{
	m_radioTopMost.SetChecked(true);
	m_radioNormal.SetChecked(false);
	ApplyAlwaysOnTop(true);
	theApp.WriteProfileInt(L"Settings", L"AlwaysOnTop", 1);
}

void CClaudePulseDlg::OnClickedRadioNormal()
{
	m_radioTopMost.SetChecked(false);
	m_radioNormal.SetChecked(true);
	ApplyAlwaysOnTop(false);
	theApp.WriteProfileInt(L"Settings", L"AlwaysOnTop", 0);
}

void CClaudePulseDlg::ApplyAlwaysOnTop(bool topMost)
{
	// wndTopMost / wndNoTopMost は MFC が用意する定数CWnd（HWND_TOPMOST / HWND_NOTOPMOST 相当）
	SetWindowPos(topMost ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void CClaudePulseDlg::OnClickedRadioSizeSmall()
{
	m_radioSizeSmall.SetChecked(true);
	m_radioSizeMedium.SetChecked(false);
	m_sizeSmall = true;
	UpdateLayout();
	theApp.WriteProfileInt(L"Settings", L"DisplaySize", 0);
}

void CClaudePulseDlg::OnClickedRadioSizeMedium()
{
	m_radioSizeSmall.SetChecked(false);
	m_radioSizeMedium.SetChecked(true);
	m_sizeSmall = false;
	UpdateLayout();
	theApp.WriteProfileInt(L"Settings", L"DisplaySize", 1);
}

void CClaudePulseDlg::OnClickedRadioDetailOff()
{
	m_radioDetailOff.SetChecked(true);
	m_radioDetailOn.SetChecked(false);
	m_detailOn = false;
	UpdateLayout();
	theApp.WriteProfileInt(L"Settings", L"DetailDisplay", 0);
}

void CClaudePulseDlg::OnClickedRadioDetailOn()
{
	m_radioDetailOff.SetChecked(false);
	m_radioDetailOn.SetChecked(true);
	m_detailOn = true;
	UpdateLayout();
	theApp.WriteProfileInt(L"Settings", L"DetailDisplay", 1);
}

void CClaudePulseDlg::CaptureBaseLayout()
{
	CRect client;
	GetClientRect(&client);
	m_baseClient = client.Size();

	m_baseRects.clear();
	for (CWnd* pChild = GetWindow(GW_CHILD); pChild != nullptr;
	     pChild = pChild->GetWindow(GW_HWNDNEXT))
	{
		CRect rc;
		pChild->GetWindowRect(&rc);
		ScreenToClient(&rc);
		m_baseRects.emplace_back(pChild->GetSafeHwnd(), rc);
	}
}

bool CClaudePulseDlg::GetBaseRect(int ctrlId, CRect& rc) const
{
	for (const auto& item : m_baseRects)
	{
		if (::IsWindow(item.first) && ::GetDlgCtrlID(item.first) == ctrlId)
		{
			rc = item.second;
			return true;
		}
	}
	return false;
}

void CClaudePulseDlg::UpdateLayout()
{
	if (m_baseRects.empty())
	{
		return;
	}

	const double scale = m_sizeSmall ? kSmallScale : 1.0;
	auto scaled = [scale](int v) { return static_cast<int>(v * scale + 0.5); };

	// 詳細表示オフで畳む高さ（基準座標系）：
	// 入力トークン行の上端から「現在のセッション」行の上端までを詰める
	// （使用制限のバー3本と監視ファイル行はその分だけ上へ移動して表示を続ける）
	int collapse = 0;
	CRect rcInput;
	CRect rcSession;
	if (!m_detailOn &&
	    GetBaseRect(IDC_STATIC_INPUT_LABEL, rcInput) &&
	    GetBaseRect(IDC_STATIC_SESSION_LABEL, rcSession))
	{
		collapse = rcSession.top - rcInput.top;
	}

	// 外形サイズ＝スケール後のクライアント領域＋非クライアント領域（枠・タイトルバー）。
	// 位置は維持し、サイズのみ変更する
	CRect wndRect, clientRect;
	GetWindowRect(&wndRect);
	GetClientRect(&clientRect);
	int ncWidth = wndRect.Width() - clientRect.Width();
	int ncHeight = wndRect.Height() - clientRect.Height();
	SetWindowPos(nullptr, 0, 0,
		scaled(m_baseClient.cx) + ncWidth,
		scaled(m_baseClient.cy - collapse) + ncHeight,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	// 子コントロールを基準（中・詳細オン）レイアウトからの比率・差分で再配置し、
	// フォントも切り替える
	HFONT hUiFont = static_cast<HFONT>(
		(m_sizeSmall ? &m_uiFontSmall : m_pDialogFont)->GetSafeHandle());
	for (const auto& item : m_baseRects)
	{
		if (!::IsWindow(item.first))
		{
			continue;
		}

		int id = ::GetDlgCtrlID(item.first);
		bool detail = IsDetailControl(id);
		if (detail && !m_detailOn)
		{
			::ShowWindow(item.first, SW_HIDE);
			continue;
		}

		CRect rc = item.second;
		if (collapse > 0)
		{
			if (id == IDC_GROUP_MONITOR)
			{
				rc.bottom -= collapse;       // 「使用状況」の枠も同じ分だけ短くする
			}
			else if (rc.top >= rcInput.top)
			{
				rc.OffsetRect(0, -collapse); // 隠した領域より下の行は畳んだ分だけ上へ詰める
			}
		}
		::MoveWindow(item.first, scaled(rc.left), scaled(rc.top),
			scaled(rc.Width()), scaled(rc.Height()), TRUE);
		::SendMessage(item.first, WM_SETFONT,
			reinterpret_cast<WPARAM>(hUiFont), TRUE);
		if (detail)
		{
			::ShowWindow(item.first, SW_SHOW);
		}
	}

	// モデル名だけは専用の大きめフォント（縮小時はその1/2版）
	GetDlgItem(IDC_STATIC_MODEL)->SetFont(m_sizeSmall ? &m_modelFontSmall : &m_modelFont);

	Invalidate();
}

void CClaudePulseDlg::OnDestroy()
{
	if (m_timerId != 0)
	{
		KillTimer(kTimerIdPoll);
		m_timerId = 0;
	}
	if (m_usageTimerId != 0)
	{
		KillTimer(kTimerIdUsage);
		m_usageTimerId = 0;
	}
	CDialogEx::OnDestroy();
}
