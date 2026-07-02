// ClaudePulseDlg.cpp : メインダイアログの実装
//

#include "pch.h"
#include "ClaudePulse.h"
#include "ClaudePulseDlg.h"

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

	// 曜日表示（CTime::GetDayOfWeek は 1=日曜）
	const LPCWSTR kWeekdays[] = { L"", L"日", L"月", L"火", L"水", L"木", L"金", L"土" };
}

// CClaudePulseDlg ダイアログ

CClaudePulseDlg::CClaudePulseDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CLAUDEPULSE_DIALOG, pParent)
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
}

BEGIN_MESSAGE_MAP(CClaudePulseDlg, CDialogEx)
	ON_WM_TIMER()
	ON_WM_CTLCOLOR()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_APP_USAGE_UPDATED, &CClaudePulseDlg::OnUsageUpdated)
END_MESSAGE_MAP()

// CClaudePulseDlg メッセージ ハンドラー

BOOL CClaudePulseDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ダークテーマの背景ブラシ
	m_backBrush.CreateSolidBrush(kColorBack);

	// モデル名用の大きめフォント（16pt / Segoe UI）
	m_modelFont.CreatePointFont(160, L"Segoe UI");
	GetDlgItem(IDC_STATIC_MODEL)->SetFont(&m_modelFont);

	// 日本語UI文字列はコードから設定する（ADR-005：.rc は ASCII のみ）
	SetWindowText(L"ClaudePulse — Claude Code 使用量モニター");
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
