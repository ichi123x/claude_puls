// ClaudePulse.cpp : アプリケーションのクラス動作を定義する
//

#include "pch.h"
#include "ClaudePulse.h"
#include "ClaudePulseDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CClaudePulseApp

BEGIN_MESSAGE_MAP(CClaudePulseApp, CWinApp)
END_MESSAGE_MAP()

// CClaudePulseApp の構築

CClaudePulseApp::CClaudePulseApp()
{
}

// 唯一の CClaudePulseApp オブジェクト

CClaudePulseApp theApp;

// CClaudePulseApp の初期化

BOOL CClaudePulseApp::InitInstance()
{
	// コモン コントロールの初期化（Visual スタイル有効時に必要）
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	CClaudePulseDlg dlg;
	m_pMainWnd = &dlg;
	dlg.DoModal();

	// ダイアログが閉じられたのでアプリを終了する
	return FALSE;
}
