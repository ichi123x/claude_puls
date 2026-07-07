// ClaudePulseDlg.h : メインダイアログのヘッダー ファイル
//
#pragma once

#include "PulseMeterCtrl.h"
#include "UsageBarCtrl.h"
#include "UsageMonitor.h"
#include "UsageLimitsClient.h"
#include "RadioDotCtrl.h"

#include <utility>
#include <vector>

// 使用量取得スレッド → UIスレッドへの結果通知メッセージ
// （LPARAM に new した UsageLimitsResult* を渡し、受信側で解放する）
constexpr UINT WM_APP_USAGE_UPDATED = WM_APP + 1;

// CClaudePulseDlg ダイアログ
class CClaudePulseDlg : public CDialogEx
{
public:
	CClaudePulseDlg(CWnd* pParent = nullptr);

// ダイアログ データ
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CLAUDEPULSE_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV サポート
	virtual BOOL OnInitDialog();

	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnUsageUpdated(WPARAM wParam, LPARAM lParam);
	afx_msg void OnClickedRadioTopMost();
	afx_msg void OnClickedRadioNormal();
	afx_msg void OnClickedRadioSizeSmall();
	afx_msg void OnClickedRadioSizeMedium();
	afx_msg void OnClickedRadioDetailOff();
	afx_msg void OnClickedRadioDetailOn();
	DECLARE_MESSAGE_MAP()

private:
	// 使用量をポーリングして画面と波形を更新する（OnInitDialog と OnTimer から呼ぶ）
	void PollAndRefresh();

	// 使用量スナップショットを画面へ反映する
	void UpdateDisplay(const UsageStats& stats);

	// プラン使用制限の取得をワーカースレッドで開始する（多重起動はしない）
	void StartUsageFetch();

	// 取得したプラン使用制限を画面へ反映する
	void ApplyUsageLimits(const UsageLimitsResult& result);

	// 使用制限1行ぶん（ラベル・バー・%）を更新する
	void UpdateLimitRow(const UsageSection& section, const CString& baseLabel,
	                    bool relativeReset, int labelId, CUsageBarCtrl& bar, int pctId);

	// ワーカースレッド本体（UsageLimitsClient::Fetch を実行して結果を PostMessage する）
	static UINT AFX_CDECL UsageFetchThread(LPVOID pParam);

	// ウィンドウの最前面表示を切り替える（ADR-007）
	void ApplyAlwaysOnTop(bool topMost);

	// 基準（中サイズ・詳細オン）のクライアント寸法と全子コントロールの矩形を記録する
	// （レイアウト変更は常にこの基準からの比率・差分で行う）
	void CaptureBaseLayout();

	// 基準矩形をコントロールIDで引く（見つからなければ false）
	bool GetBaseRect(int ctrlId, CRect& rc) const;

	// 表示サイズ（m_sizeSmall）と詳細表示（m_detailOn）の現在値でレイアウトを組み直す。
	// 小サイズは縦横1/2（面積比1/4）、詳細オフはトークン集計〜プラン使用制限を畳む。
	// 注意：識別子 small は使用禁止（rpcndr.h が small を char に #define している）
	void UpdateLayout();

	// 3桁区切りの文字列にする（例: 1234567 → "1,234,567"）
	static CString FormatWithCommas(ULONGLONG value);

	CPulseMeterCtrl m_pulseMeter;    // 心電図風パルスメータ
	CUsageBarCtrl   m_barSession;    // 現在のセッションの使用率バー
	CUsageBarCtrl   m_barWeekAll;    // 週間（すべてのモデル）の使用率バー
	CUsageBarCtrl   m_barWeekModel;  // 週間（特定モデル階級）の使用率バー
	CRadioDotCtrl   m_radioTopMost;  // 「最前面」ラジオボタン
	CRadioDotCtrl   m_radioNormal;   // 「通常」ラジオボタン
	CRadioDotCtrl   m_radioSizeSmall;  // アプリ表示サイズ「小」ラジオボタン
	CRadioDotCtrl   m_radioSizeMedium; // アプリ表示サイズ「中」ラジオボタン
	CRadioDotCtrl   m_radioDetailOff;  // 詳細表示「オフ」ラジオボタン
	CRadioDotCtrl   m_radioDetailOn;   // 詳細表示「オン」ラジオボタン
	CUsageMonitor   m_monitor;       // JSONL 監視・集計
	CFont           m_modelFont;     // モデル名表示用の大きめフォント
	CFont           m_modelFontSmall; // モデル名フォントの縮小版（表示サイズ「小」用）
	CFont           m_uiFontSmall;    // 全般ラベルの縮小フォント（表示サイズ「小」用）
	CFont*          m_pDialogFont;    // ダイアログ既定フォント（「中」へ戻すときに使う）
	CSize           m_baseClient;     // 基準（中・詳細オン）のクライアント寸法
	std::vector<std::pair<HWND, CRect>> m_baseRects; // 基準（中・詳細オン）の子コントロール矩形
	bool            m_sizeSmall;      // 表示サイズ（true=小）
	bool            m_detailOn;       // 詳細表示（true=オン）
	CBrush          m_backBrush;     // ダークテーマ背景ブラシ
	UINT_PTR        m_timerId;       // 1秒更新タイマー
	UINT_PTR        m_usageTimerId;  // 使用制限の定期取得タイマー
	bool            m_usageFetchInProgress; // 取得スレッドの多重起動防止
};
