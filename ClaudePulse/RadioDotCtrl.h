// RadioDotCtrl.h : ダークテーマ対応の自前描画ラジオボタン
//
// ビジュアルスタイル（テーマ）描画の標準ボタンは WM_CTLCOLORBTN で設定した
// 文字色を無視するため、ダークテーマの背景では文字が読めなくなる（ADR-007）。
// CStatic を派生し、円形のラジオボタン（選択状態を示すドット）とラベルを
// 自前描画することで、見た目を完全に制御する。
//
// クリックされると自分自身をチェック状態にし、親へ標準のラジオボタンと同じ
// BN_CLICKED 通知を送る。相互排他（もう一方を外す）とチェック状態の永続化は
// 呼び出し側（CClaudePulseDlg）が行う。
//
#pragma once

class CRadioDotCtrl : public CStatic
{
public:
	CRadioDotCtrl();

	void SetChecked(bool checked);
	bool IsChecked() const { return m_checked; }

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

private:
	bool m_checked;
};
