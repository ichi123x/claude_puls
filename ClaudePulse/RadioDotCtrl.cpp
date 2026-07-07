// RadioDotCtrl.cpp : ダークテーマ対応の自前描画ラジオボタン
//

#include "pch.h"
#include "RadioDotCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	// 配色（ダイアログのダークテーマに合わせる）
	const COLORREF kColorBack = RGB(11, 18, 32);    // ダイアログ背景
	const COLORREF kColorRing = RGB(140, 155, 180); // 未選択時の輪
	const COLORREF kColorDot = RGB(64, 224, 144);   // 選択時の中心ドット（緑）
	const COLORREF kColorText = RGB(230, 235, 245); // ラベル文字

	const int kDiameter = 12; // 円の直径の上限（ピクセル）。縮小表示時はコントロール高さに合わせる
}

BEGIN_MESSAGE_MAP(CRadioDotCtrl, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

CRadioDotCtrl::CRadioDotCtrl()
	: m_checked(false)
{
}

void CRadioDotCtrl::SetChecked(bool checked)
{
	if (m_checked == checked)
	{
		return;
	}
	m_checked = checked;
	if (GetSafeHwnd() != nullptr)
	{
		Invalidate(FALSE);
	}
}

BOOL CRadioDotCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
	// ちらつき防止：背景消去は OnPaint 内で行う
	return TRUE;
}

void CRadioDotCtrl::OnPaint()
{
	CPaintDC dc(this);
	CRect rc;
	GetClientRect(&rc);
	if (rc.IsRectEmpty())
	{
		return;
	}

	dc.FillSolidRect(&rc, kColorBack);
	dc.SetBkMode(TRANSPARENT);

	// 表示サイズ「小」でコントロールが低くなっても円がはみ出さないようにする
	int diameter = (rc.Height() < kDiameter) ? rc.Height() : kDiameter;
	int top = rc.top + (rc.Height() - diameter) / 2;
	CRect ring(rc.left, top, rc.left + diameter, top + diameter);

	// 円の輪（塗りつぶしなし）。生のGDIハンドルを使い、選択解除→削除の順序を
	// 確実に守る（選択中のGDIオブジェクトを破棄するとデバッグアサーションの原因になる）
	HDC hdc = dc.GetSafeHdc();
	HPEN hRingPen = ::CreatePen(PS_SOLID, 1, kColorRing);
	HGDIOBJ hOldPen = ::SelectObject(hdc, hRingPen);
	HGDIOBJ hOldBrush = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
	::Ellipse(hdc, ring.left, ring.top, ring.right, ring.bottom);
	::SelectObject(hdc, hOldBrush);
	::SelectObject(hdc, hOldPen);
	::DeleteObject(hRingPen);

	if (m_checked)
	{
		// 中心ドットの余白は直径に比例させる（12px時に3px相当）
		CRect dot(ring);
		int inset = (diameter + 2) / 4;
		dot.DeflateRect(inset, inset);

		HBRUSH hDotBrush = ::CreateSolidBrush(kColorDot);
		HGDIOBJ hOldDotBrush = ::SelectObject(hdc, hDotBrush);
		HGDIOBJ hOldDotPen = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
		::Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
		::SelectObject(hdc, hOldDotPen);
		::SelectObject(hdc, hOldDotBrush);
		::DeleteObject(hDotBrush);
	}

	// ラベルはコントロールに設定されたフォントで描く（表示サイズ切替に追従させる）
	CString label;
	GetWindowText(label);
	CRect textRc(ring.right + 6, rc.top, rc.right, rc.bottom);
	CFont* pFont = GetFont();
	CFont* pOldFont = (pFont != nullptr) ? dc.SelectObject(pFont) : nullptr;
	dc.SetTextColor(kColorText);
	dc.DrawText(label, textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	if (pOldFont != nullptr)
	{
		dc.SelectObject(pOldFont);
	}
}

void CRadioDotCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetChecked(true);

	// 標準のラジオボタンと同じ通知にすることで、既存の ON_BN_CLICKED
	// ハンドラー（CClaudePulseDlg 側）をそのまま使えるようにする
	CWnd* pParent = GetParent();
	if (pParent != nullptr)
	{
		pParent->SendMessage(WM_COMMAND,
			MAKEWPARAM(GetDlgCtrlID(), BN_CLICKED),
			reinterpret_cast<LPARAM>(GetSafeHwnd()));
	}

	CStatic::OnLButtonDown(nFlags, point);
}
