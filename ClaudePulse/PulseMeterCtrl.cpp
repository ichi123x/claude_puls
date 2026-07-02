// PulseMeterCtrl.cpp : 心電図風のパルスメータ描画コントロール
//

#include "pch.h"
#include "PulseMeterCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	// 配色（ダークテーマ）
	const COLORREF kColorBack = RGB(11, 18, 32);    // 背景（濃紺）
	const COLORREF kColorGrid = RGB(28, 42, 66);    // グリッド線
	const COLORREF kColorWaveGlow = RGB(16, 96, 64);   // 波形の残光（太い下地）
	const COLORREF kColorWave = RGB(64, 224, 144);  // 波形本体（緑）
	const COLORREF kColorBaseline = RGB(40, 72, 56);    // 基準線

	const int kGridStep = 16;   // グリッド間隔（ピクセル）
	const int kSampleStep = 4;  // 1サンプルあたりの横幅（ピクセル）
}

BEGIN_MESSAGE_MAP(CPulseMeterCtrl, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()

CPulseMeterCtrl::CPulseMeterCtrl()
	: m_maxSamples(256)
	, m_idlePhase(0)
{
}

void CPulseMeterCtrl::AddSample(ULONGLONG deltaTokens, bool active)
{
	double v = 0.0;

	if (deltaTokens > 0)
	{
		v = Normalize(deltaTokens);
		m_idlePhase = 0;
	}
	else if (active)
	{
		// アイドル中も「生きている」ことが分かるよう、8サンプル周期で小さな脈を打つ
		++m_idlePhase;
		if ((m_idlePhase % 8) == 0)
		{
			v = 0.06;
		}
	}

	m_samples.push_back(v);
	while (m_samples.size() > m_maxSamples)
	{
		m_samples.pop_front();
	}

	if (GetSafeHwnd() != nullptr)
	{
		Invalidate(FALSE);
	}
}

double CPulseMeterCtrl::Normalize(ULONGLONG deltaTokens)
{
	// 増分は数トークン〜数万トークンまで桁が大きく変わるため対数スケールにする。
	// log10(1+n) / 4 → 約10,000トークンで 1.0（振り切れ）
	double v = std::log10(1.0 + static_cast<double>(deltaTokens)) / 4.0;
	return (std::min)(1.0, (std::max)(0.0, v));
}

BOOL CPulseMeterCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
	// ちらつき防止：背景消去は OnPaint 内のダブルバッファで行う
	return TRUE;
}

void CPulseMeterCtrl::OnSize(UINT nType, int cx, int cy)
{
	CStatic::OnSize(nType, cx, cy);

	// 横幅に合わせて保持サンプル数を調整する（波形バッファは維持）
	if (cx > 0)
	{
		m_maxSamples = static_cast<size_t>(cx / kSampleStep) + 2;
	}
	Invalidate(FALSE);
}

void CPulseMeterCtrl::OnPaint()
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);
	if (rc.IsRectEmpty())
	{
		return;
	}

	// メモリDCに描いてから一括転送する（ダブルバッファ）
	CDC memDC;
	memDC.CreateCompatibleDC(&dc);
	CBitmap bmp;
	bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
	CBitmap* pOldBmp = memDC.SelectObject(&bmp);

	DrawBackground(memDC, rc);
	DrawWaveform(memDC, rc);

	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
	memDC.SelectObject(pOldBmp);
}

void CPulseMeterCtrl::DrawBackground(CDC& dc, const CRect& rc) const
{
	dc.FillSolidRect(&rc, kColorBack);

	// 心電図モニター風のグリッド
	CPen gridPen(PS_SOLID, 1, kColorGrid);
	CPen* pOldPen = dc.SelectObject(&gridPen);

	for (int x = rc.left; x < rc.right; x += kGridStep)
	{
		dc.MoveTo(x, rc.top);
		dc.LineTo(x, rc.bottom);
	}
	for (int y = rc.top; y < rc.bottom; y += kGridStep)
	{
		dc.MoveTo(rc.left, y);
		dc.LineTo(rc.right, y);
	}

	// 基準線（波形のゼロ位置）
	int baseY = rc.bottom - rc.Height() / 6;
	CPen basePen(PS_SOLID, 1, kColorBaseline);
	dc.SelectObject(&basePen);
	dc.MoveTo(rc.left, baseY);
	dc.LineTo(rc.right, baseY);

	dc.SelectObject(pOldPen);
}

void CPulseMeterCtrl::DrawWaveform(CDC& dc, const CRect& rc) const
{
	if (m_samples.empty())
	{
		return;
	}

	int baseY = rc.bottom - rc.Height() / 6;          // ゼロ位置
	int amplitude = baseY - rc.top - 4;                // 最大振幅（ピクセル）

	// 右端が最新サンプルになるよう座標列を作る
	std::vector<POINT> points;
	points.reserve(m_samples.size());

	int x = rc.right - static_cast<int>(m_samples.size() - 1) * kSampleStep - 2;
	for (double v : m_samples)
	{
		POINT pt;
		pt.x = x;
		pt.y = baseY - static_cast<int>(v * amplitude);
		points.push_back(pt);
		x += kSampleStep;
	}

	// 残光（太い暗緑）→ 本体（細い明緑）の順に重ね描きして発光風にする
	CPen glowPen(PS_SOLID, 3, kColorWaveGlow);
	CPen* pOldPen = dc.SelectObject(&glowPen);
	dc.Polyline(points.data(), static_cast<int>(points.size()));

	CPen wavePen(PS_SOLID, 1, kColorWave);
	dc.SelectObject(&wavePen);
	dc.Polyline(points.data(), static_cast<int>(points.size()));

	// 最新点に輝点を打つ
	const POINT& last = points.back();
	CBrush dotBrush(kColorWave);
	CBrush* pOldBrush = dc.SelectObject(&dotBrush);
	CPen dotPen(PS_SOLID, 1, kColorWave);
	dc.SelectObject(&dotPen);
	dc.Ellipse(last.x - 2, last.y - 2, last.x + 3, last.y + 3);

	dc.SelectObject(pOldBrush);
	dc.SelectObject(pOldPen);
}
