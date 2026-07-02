// PulseMeterCtrl.h : 心電図風のパルスメータ描画コントロール
//
// CStatic を派生し、WM_PAINT を自前描画（ダブルバッファ）で置き換える（ADR-004）。
// 1秒ごとに AddSample() で増分トークン数を受け取り、右端に新しいサンプルを
// 追加しながら左へスクロールする波形を描く。
//
#pragma once

#include <deque>

class CPulseMeterCtrl : public CStatic
{
public:
	CPulseMeterCtrl();

	// 増分トークン数を1サンプルとして追加する（アクティブなら true）
	void AddSample(ULONGLONG deltaTokens, bool active);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

private:
	// トークン増分を 0.0〜1.0 に正規化する（対数スケール：10,000 tok/s で振り切れ）
	static double Normalize(ULONGLONG deltaTokens);

	void DrawBackground(CDC& dc, const CRect& rc) const;
	void DrawWaveform(CDC& dc, const CRect& rc) const;

	std::deque<double> m_samples;   // 正規化済みサンプル（0.0〜1.0）。先頭が最古
	size_t             m_maxSamples;
	int                m_idlePhase; // アイドル時に小さな心拍を刻むための位相
};
