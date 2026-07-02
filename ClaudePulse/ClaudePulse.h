// ClaudePulse.h : アプリケーションのメイン ヘッダー ファイル
//
#pragma once

#ifndef __AFXWIN_H__
	#error "PCH に対してこのファイルをインクルードする前に 'pch.h' をインクルードしてください"
#endif

#include "Resource.h"

// CClaudePulseApp:
// このクラスの実装については ClaudePulse.cpp を参照
class CClaudePulseApp : public CWinApp
{
public:
	CClaudePulseApp();

// オーバーライド
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};

extern CClaudePulseApp theApp;
