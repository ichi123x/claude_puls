// JsonLite.h : 軽量JSON値抽出ヘルパー
//
// 外部JSONライブラリを使わない方針（ADR-003）のための共通ヘルパー。
// キー文字列検索による抽出のみを行い、厳密なJSONパースはしない。
// UsageMonitor（JSONL解析）と UsageLimitsClient（使用量API応答解析）で共用する。
//
#pragma once

#include <string>

namespace JsonLite
{

// "key":"value" 形式の文字列値を抽出する
inline bool ExtractString(const std::string& json, const char* key, std::string& out)
{
	std::string pattern = std::string("\"") + key + "\":\"";
	size_t pos = json.find(pattern);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos += pattern.length();

	out.clear();
	while (pos < json.length())
	{
		char c = json[pos];
		if (c == '\\' && pos + 1 < json.length())
		{
			// エスケープされた文字はそのまま取り込む
			out += json[pos + 1];
			pos += 2;
			continue;
		}
		if (c == '"')
		{
			return true;
		}
		out += c;
		++pos;
	}
	return false;
}

// "key":123 形式の非負整数値を抽出する。
// キーは前の二重引用符まで含めて検索するため、"input_tokens" が
// "cache_read_input_tokens" に誤一致することはない
inline bool ExtractUInt(const std::string& json, const char* key, ULONGLONG& out)
{
	std::string pattern = std::string("\"") + key + "\":";
	size_t pos = json.find(pattern);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos += pattern.length();

	// 空白を読み飛ばして数字列を取り出す
	while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t'))
	{
		++pos;
	}
	if (pos >= json.length() || json[pos] < '0' || json[pos] > '9')
	{
		return false;
	}

	ULONGLONG value = 0;
	while (pos < json.length() && json[pos] >= '0' && json[pos] <= '9')
	{
		value = value * 10 + static_cast<ULONGLONG>(json[pos] - '0');
		++pos;
	}
	out = value;
	return true;
}

// "key":12.5 形式の数値（整数・小数）を抽出する
inline bool ExtractDouble(const std::string& json, const char* key, double& out)
{
	std::string pattern = std::string("\"") + key + "\":";
	size_t pos = json.find(pattern);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos += pattern.length();

	while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t'))
	{
		++pos;
	}

	std::string token;
	while (pos < json.length() &&
	       ((json[pos] >= '0' && json[pos] <= '9') ||
	        json[pos] == '.' || json[pos] == '-' || json[pos] == '+' ||
	        json[pos] == 'e' || json[pos] == 'E'))
	{
		token += json[pos];
		++pos;
	}
	if (token.empty())
	{
		return false;
	}
	out = atof(token.c_str());
	return true;
}

// "key":{ ... } 形式のオブジェクト部分（波括弧含む）を抽出する。
// 波括弧の対応を数える簡易版（文字列リテラル内の '{' は考慮しないが、
// 対象の応答には現れない前提）
inline bool ExtractObject(const std::string& json, const char* key, std::string& out)
{
	std::string pattern = std::string("\"") + key + "\":";
	size_t pos = json.find(pattern);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos += pattern.length();

	while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t'))
	{
		++pos;
	}
	if (pos >= json.length() || json[pos] != '{')
	{
		return false;
	}

	int depth = 0;
	size_t start = pos;
	for (; pos < json.length(); ++pos)
	{
		if (json[pos] == '{')
		{
			++depth;
		}
		else if (json[pos] == '}')
		{
			--depth;
			if (depth == 0)
			{
				out = json.substr(start, pos - start + 1);
				return true;
			}
		}
	}
	return false;
}

} // namespace JsonLite
