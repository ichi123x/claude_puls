// UsageBarCtrl.cpp : 使用率プログレスバー（Claude Desktop 風）
//

#include "pch.h"
#include "UsageBarCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	// 配色（ダイアログのダークテーマに合わせる）
	const COLORREF kColorBack = RGB(11, 18, 32);     // ダイアログ背景
	const COLORREF kColorTrack = RGB(45, 55, 75);     // トラック（未使用部分）
	const COLORREF kColorNormal = RGB(59, 130, 246);   // 〜70%：青
	const COLORREF kColorWarn = RGB(245, 158, 11);   // 70%〜：オレンジ
	const COLORREF kColorDanger = RGB(239, 68, 68);    // 90%〜：赤
}

BEGIN_MESSAGE_MAP(CUsageBarCtrl, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CUsageBarCtrl::CUsageBarCtrl()
	: m_percent(-1.0)
{
}

void CUsageBarCtrl::SetPercent(double percent)
{
	m_percent = percent;
	if (GetSafeHwnd() != nullptr)
	{
		Invalidate(FALSE);
	}
}

BOOL CUsageBarCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
	// ちらつき防止：背景消去は OnPaint 内で行う
	return TRUE;
}

void CUsageBarCtrl::OnPaint()
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

	memDC.FillSolidRect(&rc, kColorBack);

	int radius = rc.Height();  // 高さいっぱいの角丸

	// トラック（全幅）
	{
		CBrush trackBrush(kColorTrack);
		CPen trackPen(PS_SOLID, 1, kColorTrack);
		CBrush* pOldBrush = memDC.SelectObject(&trackBrush);
		CPen* pOldPen = memDC.SelectObject(&trackPen);
		memDC.RoundRect(rc.left, rc.top, rc.right, rc.bottom, radius, radius);

		// バー（使用率ぶんの幅。取得済みのときのみ）
		if (m_percent >= 0.0)
		{
			COLORREF fill = kColorNormal;
			if (m_percent >= 90.0)
			{
				fill = kColorDanger;
			}
			else if (m_percent >= 70.0)
			{
				fill = kColorWarn;
			}

			int width = static_cast<int>(rc.Width() * ((std::min)(100.0, m_percent) / 100.0));
			if (width > 0)
			{
				// 角丸が潰れないよう最小幅を確保する
				width = (std::max)(width, rc.Height());

				CBrush fillBrush(fill);
				CPen fillPen(PS_SOLID, 1, fill);
				memDC.SelectObject(&fillBrush);
				memDC.SelectObject(&fillPen);
				memDC.RoundRect(rc.left, rc.top, rc.left + width, rc.bottom, radius, radius);
			}
		}

		memDC.SelectObject(pOldBrush);
		memDC.SelectObject(pOldPen);
	}

	dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
	memDC.SelectObject(pOldBmp);
}
