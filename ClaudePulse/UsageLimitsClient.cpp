// UsageLimitsClient.cpp : プラン使用制限（使用率%・リセット時刻）の取得
//

#include "pch.h"
#include "UsageLimitsClient.h"
#include "JsonLite.h"
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

bool CUsageLimitsClient::Fetch(UsageLimitsResult& out)
{
	out = UsageLimitsResult();

	CString accessToken;
	if (!ReadCredentials(accessToken, out.planName, out.errorMessage))
	{
		return false;
	}

	std::string body;
	DWORD statusCode = 0;
	if (!HttpGetUsage(accessToken, body, statusCode, out.errorMessage))
	{
		return false;
	}

	if (statusCode == 401 || statusCode == 403)
	{
		out.errorMessage = L"認証の期限切れです。Claude Code を一度起動して更新してください";
		return false;
	}
	if (statusCode != 200)
	{
		out.errorMessage.Format(L"使用量の取得に失敗しました（HTTP %lu）", statusCode);
		return false;
	}

	// 現在のセッション（5時間枠）と週間制限を抽出する
	ParseSection(body, "five_hour", out.fiveHour);
	ParseSection(body, "seven_day", out.sevenDay);

	// 特定モデル階級の週間制限。キー名はモデル世代で変わるため候補順に探す
	ParseSection(body, "seven_day_fable", out.sevenDayModel);
	if (out.sevenDayModel.present)
	{
		out.sevenDayModelLabel = L"Fable";
	}
	else
	{
		ParseSection(body, "seven_day_opus", out.sevenDayModel);
		if (out.sevenDayModel.present)
		{
			out.sevenDayModelLabel = L"Opus";
		}
	}

	if (!out.fiveHour.present && !out.sevenDay.present && !out.sevenDayModel.present)
	{
		out.errorMessage = L"使用量の応答形式を解釈できませんでした";
		return false;
	}

	out.fetchedAt = CTime::GetCurrentTime();
	out.ok = true;
	return true;
}

bool CUsageLimitsClient::ReadCredentials(CString& accessToken, CString& planName, CString& error)
{
	WCHAR profile[MAX_PATH] = {};
	DWORD len = ::GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
	if (len == 0 || len >= MAX_PATH)
	{
		error = L"USERPROFILE を解決できませんでした";
		return false;
	}

	CString path;
	path.Format(L"%s\\.claude\\.credentials.json", profile);

	CFile file;
	if (!file.Open(path, CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary))
	{
		error = L"認証情報がありません。Claude Code でログインしてください";
		return false;
	}

	ULONGLONG length = file.GetLength();
	if (length == 0 || length > 1024 * 1024)
	{
		error = L"認証情報ファイルを読み取れませんでした";
		return false;
	}

	std::string json;
	json.resize(static_cast<size_t>(length));
	file.Read(&json[0], static_cast<UINT>(length));

	std::string token;
	if (!JsonLite::ExtractString(json, "accessToken", token) || token.empty())
	{
		error = L"アクセストークンが見つかりません。Claude Code でログインしてください";
		return false;
	}
	accessToken = CString(CA2W(token.c_str(), CP_UTF8));

	// プラン種別（pro / max など）。先頭を大文字にして表示用にする
	std::string plan;
	if (JsonLite::ExtractString(json, "subscriptionType", plan) && !plan.empty())
	{
		planName = CString(CA2W(plan.c_str(), CP_UTF8));
		planName.SetAt(0, static_cast<WCHAR>(towupper(planName[0])));
	}
	return true;
}

bool CUsageLimitsClient::HttpGetUsage(const CString& accessToken, std::string& body,
                                      DWORD& statusCode, CString& error)
{
	body.clear();
	statusCode = 0;

	HINTERNET hSession = ::WinHttpOpen(L"ClaudePulse/1.0",
		WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (hSession == nullptr)
	{
		error = L"WinHTTP の初期化に失敗しました";
		return false;
	}

	bool result = false;
	HINTERNET hConnect = ::WinHttpConnect(hSession, L"api.anthropic.com",
		INTERNET_DEFAULT_HTTPS_PORT, 0);
	HINTERNET hRequest = nullptr;

	if (hConnect != nullptr)
	{
		hRequest = ::WinHttpOpenRequest(hConnect, L"GET", L"/api/oauth/usage",
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE);
	}

	if (hRequest != nullptr)
	{
		// タイムアウト（ミリ秒）：解決 / 接続 / 送信 / 受信
		::WinHttpSetTimeouts(hRequest, 5000, 5000, 10000, 10000);

		CString headers;
		headers.Format(
			L"Authorization: Bearer %s\r\n"
			L"anthropic-beta: oauth-2021-06-01\r\n"
			L"Accept: application/json",
			(LPCWSTR)accessToken);

		if (::WinHttpSendRequest(hRequest, headers, static_cast<DWORD>(-1),
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
			::WinHttpReceiveResponse(hRequest, nullptr))
		{
			DWORD size = sizeof(statusCode);
			::WinHttpQueryHeaders(hRequest,
				WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size,
				WINHTTP_NO_HEADER_INDEX);

			// 応答本文を読み切る
			DWORD available = 0;
			while (::WinHttpQueryDataAvailable(hRequest, &available) && available > 0)
			{
				std::string chunk(available, '\0');
				DWORD read = 0;
				if (!::WinHttpReadData(hRequest, &chunk[0], available, &read) || read == 0)
				{
					break;
				}
				body.append(chunk.data(), read);
			}
			result = true;
		}
		else
		{
			error.Format(L"接続できませんでした（エラー %lu）", ::GetLastError());
		}
	}
	else
	{
		error = L"HTTP リクエストを作成できませんでした";
	}

	if (hRequest != nullptr)
	{
		::WinHttpCloseHandle(hRequest);
	}
	if (hConnect != nullptr)
	{
		::WinHttpCloseHandle(hConnect);
	}
	::WinHttpCloseHandle(hSession);
	return result;
}

void CUsageLimitsClient::ParseSection(const std::string& body, const char* key, UsageSection& out)
{
	out = UsageSection();

	std::string obj;
	if (!JsonLite::ExtractObject(body, key, obj))
	{
		return;
	}

	double utilization = 0.0;
	if (!JsonLite::ExtractDouble(obj, "utilization", utilization))
	{
		return;
	}

	out.present = true;
	out.utilization = (std::min)(100.0, (std::max)(0.0, utilization));

	std::string resetsAt;
	if (JsonLite::ExtractString(obj, "resets_at", resetsAt))
	{
		out.hasReset = ParseIso8601ToLocalTime(resetsAt, out.resetTime);
	}
}

bool CUsageLimitsClient::ParseIso8601ToLocalTime(const std::string& iso, CTime& out)
{
	// 例: "2026-07-07T13:59:59.000000+00:00" / "2026-07-07T13:59:59Z"（UTC前提）
	int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
	if (sscanf_s(iso.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) < 6)
	{
		return false;
	}

	SYSTEMTIME utc = {};
	utc.wYear = static_cast<WORD>(y);
	utc.wMonth = static_cast<WORD>(mo);
	utc.wDay = static_cast<WORD>(d);
	utc.wHour = static_cast<WORD>(h);
	utc.wMinute = static_cast<WORD>(mi);
	utc.wSecond = static_cast<WORD>(s);

	SYSTEMTIME local = {};
	if (!::SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local))
	{
		return false;
	}

	out = CTime(local);
	return true;
}
