// UsageLimitsClient.h : プラン使用制限（使用率%・リセット時刻）の取得
//
// Claude Desktop の設定画面と同じ情報を表示するため、Claude Code がログイン時に
// 保存する OAuth アクセストークン（%USERPROFILE%\.claude\.credentials.json）を
// 読み取り専用で利用し、使用量エンドポイント
//   GET https://api.anthropic.com/api/oauth/usage
// を WinHTTP で呼び出す。通信はこの1エンドポイントのみ（ADR-006）。
//
// Fetch() はブロッキングするため、UIスレッドから直接呼ばず
// ワーカースレッドから呼ぶこと（ClaudePulseDlg::UsageFetchThread 参照）。
//
#pragma once

#include <string>

// 使用制限の1区分（現在のセッション／週間など）
struct UsageSection
{
	bool   present = false;    // 応答にこの区分が含まれていたか
	double utilization = 0.0;  // 使用率（0〜100）
	bool   hasReset = false;   // リセット時刻を取得できたか
	CTime  resetTime;          // リセット時刻（ローカル時刻）
};

// 使用量エンドポイントの取得結果
struct UsageLimitsResult
{
	bool    ok = false;
	CString errorMessage;       // ok == false のときの表示用メッセージ
	CString planName;           // プラン名（Pro / Max など。不明なら空）
	UsageSection fiveHour;      // 現在のセッション（5時間枠）
	UsageSection sevenDay;      // 週間制限（すべてのモデル）
	UsageSection sevenDayModel; // 週間制限（特定モデル階級）
	CString sevenDayModelLabel; // 上記のラベル（Fable / Opus）
	CTime   fetchedAt;          // 取得時刻
};

class CUsageLimitsClient
{
public:
	// 使用量を同期取得する（ワーカースレッドから呼ぶこと）
	static bool Fetch(UsageLimitsResult& out);

private:
	static bool ReadCredentials(CString& accessToken, CString& planName, CString& error);
	static bool HttpGetUsage(const CString& accessToken, std::string& body,
	                         DWORD& statusCode, CString& error);
	static void ParseSection(const std::string& body, const char* key, UsageSection& out);
	static bool ParseIso8601ToLocalTime(const std::string& iso, CTime& out);
};
