// UsageBarCtrl.h : 使用率プログレスバー（Claude Desktop 風）
//
// CStatic を派生し、角丸のトラックと使用率に応じた色のバーを自前描画する。
// 使用率に応じて色が変わる（通常：青 / 70%以上：オレンジ / 90%以上：赤）。
//
#pragma once

class CUsageBarCtrl : public CStatic
{
public:
	CUsageBarCtrl();

	// 使用率（0〜100）を設定する。負の値は「未取得」としてトラックのみ描画
	void SetPercent(double percent);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	DECLARE_MESSAGE_MAP()

private:
	double m_percent;   // 使用率（0〜100）。負なら未取得
};
