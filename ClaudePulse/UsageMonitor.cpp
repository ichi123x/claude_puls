// UsageMonitor.cpp : Claude Code のセッション記録（JSONL）を監視し、使用量を集計する
//

#include "pch.h"
#include "UsageMonitor.h"
#include "JsonLite.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CUsageMonitor::CUsageMonitor()
	: m_offset(0)
	, m_prevTotal(0)
{
}

bool CUsageMonitor::Initialize()
{
	// %USERPROFILE%\.claude\projects を監視対象のルートとする（ADR-002）
	WCHAR profile[MAX_PATH] = {};
	DWORD len = ::GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
	if (len == 0 || len >= MAX_PATH)
	{
		return false;
	}

	m_projectsDir.Format(L"%s\\.claude\\projects", profile);

	DWORD attr = ::GetFileAttributesW(m_projectsDir);
	return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

void CUsageMonitor::Poll(UsageStats& out)
{
	CString latest = FindLatestJsonl();
	if (latest.IsEmpty())
	{
		// 監視対象なし：待機状態として返す（累計はクリアしない）
		m_stats.valid = false;
		m_stats.deltaTokens = 0;
		out = m_stats;
		return;
	}

	bool switched = (latest.CompareNoCase(m_currentFile) != 0);
	if (switched)
	{
		// 新しいセッションに切り替わった：累計を作り直す
		m_currentFile = latest;
		m_offset = 0;
		m_pending.clear();
		m_lastMessageId.clear();
		m_stats = UsageStats();
	}

	ReadNewData();

	m_stats.valid = true;
	m_stats.watchedFile = m_currentFile;

	// 前回ポーリングからの増分を計算する（パルス波形用）。
	// ファイル切替直後は過去分を一括で読むため、増分は 0 として脈飛びを防ぐ
	ULONGLONG total = m_stats.Total();
	m_stats.deltaTokens = (!switched && total >= m_prevTotal) ? (total - m_prevTotal) : 0;
	m_prevTotal = total;

	out = m_stats;
}

CString CUsageMonitor::FindLatestJsonl() const
{
	if (m_projectsDir.IsEmpty())
	{
		return CString();
	}

	CString bestPath;
	FILETIME bestTime = {};
	FindLatestRecursive(m_projectsDir, bestPath, bestTime);
	return bestPath;
}

void CUsageMonitor::FindLatestRecursive(const CString& dir, CString& bestPath, FILETIME& bestTime) const
{
	CFileFind finder;
	BOOL working = finder.FindFile(dir + L"\\*");
	while (working)
	{
		working = finder.FindNextFile();

		if (finder.IsDots())
		{
			continue;
		}

		if (finder.IsDirectory())
		{
			FindLatestRecursive(finder.GetFilePath(), bestPath, bestTime);
			continue;
		}

		CString name = finder.GetFileName();
		if (name.GetLength() < 6 || name.Right(6).CompareNoCase(L".jsonl") != 0)
		{
			continue;
		}

		FILETIME ft = {};
		if (finder.GetLastWriteTime(&ft) && ::CompareFileTime(&ft, &bestTime) > 0)
		{
			bestTime = ft;
			bestPath = finder.GetFilePath();
		}
	}
	finder.Close();
}

void CUsageMonitor::ReadNewData()
{
	CFile file;
	CFileException ex;
	// Claude Code が書き込み中でも読めるよう共有モードで開く
	if (!file.Open(m_currentFile,
	               CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary, &ex))
	{
		return;
	}

	ULONGLONG length = file.GetLength();
	if (length < m_offset)
	{
		// ファイルが縮んだ（作り直された）場合は先頭から読み直す
		m_offset = 0;
		m_pending.clear();
		m_lastMessageId.clear();
		m_stats = UsageStats();
		m_prevTotal = 0;
	}
	if (length == m_offset)
	{
		return;
	}

	file.Seek(static_cast<LONGLONG>(m_offset), CFile::begin);

	char buffer[64 * 1024];
	UINT read = 0;
	while ((read = file.Read(buffer, sizeof(buffer))) > 0)
	{
		m_pending.append(buffer, read);
		m_offset += read;

		// 完結した行（'\n' まで）だけを解析し、残りは次回へ持ち越す
		size_t pos = 0;
		size_t nl = 0;
		while ((nl = m_pending.find('\n', pos)) != std::string::npos)
		{
			ParseLine(m_pending.substr(pos, nl - pos));
			pos = nl + 1;
		}
		m_pending.erase(0, pos);
	}
}

void CUsageMonitor::ParseLine(const std::string& line)
{
	// assistant 行（モデル応答）以外は使用量を持たないため読み飛ばす
	if (line.find("\"type\":\"assistant\"") == std::string::npos ||
	    line.find("\"usage\"") == std::string::npos)
	{
		return;
	}

	// 同一メッセージIDの行が複数回書かれることがあるため、二重加算を防ぐ
	std::string messageId;
	if (JsonLite::ExtractString(line, "id", messageId) && !messageId.empty())
	{
		if (messageId == m_lastMessageId)
		{
			return;
		}
		m_lastMessageId = messageId;
	}

	// モデル名（合成応答 "<synthetic>" は表示しない）
	std::string model;
	if (JsonLite::ExtractString(line, "model", model) && !model.empty() && model[0] != '<')
	{
		m_stats.modelName = CString(CA2W(model.c_str(), CP_UTF8));
	}

	// 各トークン数を累計へ加算する
	ULONGLONG v = 0;
	if (JsonLite::ExtractUInt(line, "input_tokens", v))
	{
		m_stats.inputTokens += v;
	}
	if (JsonLite::ExtractUInt(line, "output_tokens", v))
	{
		m_stats.outputTokens += v;
	}
	if (JsonLite::ExtractUInt(line, "cache_read_input_tokens", v))
	{
		m_stats.cacheReadTokens += v;
	}
	if (JsonLite::ExtractUInt(line, "cache_creation_input_tokens", v))
	{
		m_stats.cacheCreationTokens += v;
	}
}
