// UsageMonitor.h : Claude Code のセッション記録（JSONL）を監視し、使用量を集計する
//
// データソース: %USERPROFILE%\.claude\projects\**\*.jsonl
// Claude Code は応答のたびに assistant 行を追記し、そこに "model" と
// "usage"（input_tokens / output_tokens / cache_read_input_tokens /
// cache_creation_input_tokens）が含まれる。
// 本クラスは最終更新が最新のファイルを差分読み（追尾）して累計する。
// 詳細な設計判断は .steering/decisions.md の ADR-002 / ADR-003 を参照。
//
#pragma once

#include <string>

// 使用量の集計結果（ポーリングごとにダイアログへ渡すスナップショット）
struct UsageStats
{
	CString   modelName;               // 直近の assistant 行のモデル名（例: claude-fable-5）
	ULONGLONG inputTokens = 0;         // 入力トークン累計
	ULONGLONG outputTokens = 0;        // 出力トークン累計
	ULONGLONG cacheReadTokens = 0;     // キャッシュ読込トークン累計
	ULONGLONG cacheCreationTokens = 0; // キャッシュ作成トークン累計
	ULONGLONG deltaTokens = 0;         // 前回ポーリングからの増分（パルス波形用）
	CString   watchedFile;             // 現在監視中の JSONL のフルパス
	bool      valid = false;           // 監視対象が見つかっているか

	// 全種別の合計トークン数
	ULONGLONG Total() const
	{
		return inputTokens + outputTokens + cacheReadTokens + cacheCreationTokens;
	}
};

class CUsageMonitor
{
public:
	CUsageMonitor();

	// %USERPROFILE%\.claude\projects の場所を解決する。
	// フォルダが存在しなくても false を返すだけで例外は投げない
	bool Initialize();

	// 最新の JSONL を特定して差分を読み、集計結果を out に返す（1秒タイマーから呼ぶ）
	void Poll(UsageStats& out);

private:
	// projects 配下で最終更新が最も新しい .jsonl のフルパスを返す（無ければ空文字）
	CString FindLatestJsonl() const;
	void FindLatestRecursive(const CString& dir, CString& bestPath, FILETIME& bestTime) const;

	// ファイルを offset から末尾まで読み、完結した行だけを解析する
	void ReadNewData();

	// JSONL の1行を解析し、assistant 行なら使用量を累計へ加算する
	void ParseLine(const std::string& line);

	CString     m_projectsDir;   // %USERPROFILE%\.claude\projects
	CString     m_currentFile;   // 追尾中の JSONL
	ULONGLONG   m_offset;        // 読み取り済みバイト位置
	std::string m_pending;       // 行の途中で切れた読み残しバッファ
	std::string m_lastMessageId; // 直前に集計したメッセージID（重複行の二重加算防止）
	UsageStats  m_stats;         // 累計値
	ULONGLONG   m_prevTotal;     // 前回ポーリング時の合計（増分計算用）
};
