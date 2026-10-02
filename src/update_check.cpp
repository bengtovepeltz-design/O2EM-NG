// src/update_check.cpp
//
// Phase 2: check whether a newer PUBLIC O2EM-NG GitHub release exists.
// LOOK, COMPARE, REPORT — this module never downloads or installs anything.
//
// Release discovery note (verified against the live repository, 2026-09-18):
// v0.31.0-beta is published with prerelease=true, and GitHub's
// /releases/latest endpoint returns ONLY non-prerelease releases — it
// answers 404 for this repository. The smallest reliable public-release
// query that fits the project's actual release practice is therefore
//
//     GET /repos/<owner>/<repo>/releases?per_page=1
//
// (newest published release first; unauthenticated responses never include
// drafts). See https://docs.github.com/en/rest/releases/releases.
//
#include "src/update_check.h"

#include <windows.h>
#include <winhttp.h>

#include <cstring>
#include <mutex>
#include <string>
#include <utility>

#include "src/version.h"

#pragma comment(lib, "winhttp.lib")

namespace
{
// Public release source (design study, confirmed live):
// https://github.com/bengtovepeltz-design/O2EM-NG
constexpr wchar_t kApiHost[] = L"api.github.com";
constexpr wchar_t kReleasesPath[] =
    L"/repos/bengtovepeltz-design/O2EM-NG/releases?per_page=1";
constexpr wchar_t kRepoPageBase[] =
    L"https://github.com/bengtovepeltz-design/O2EM-NG/releases/tag/";

// Wide counterpart of O2EM_VERSION_STRING for the User-Agent header.
#define O2EM_WSTRINGIFY_IMPL(x) L#x
#define O2EM_WSTRINGIFY(x) O2EM_WSTRINGIFY_IMPL(x)
const wchar_t* BuildUserAgent()
{
    static const std::wstring agent =
        L"O2EM-NG-UpdateCheck/" +
        std::to_wstring(O2EM_VERSION_MAJOR) + L"." +
        std::to_wstring(O2EM_VERSION_MINOR) + L"." +
        std::to_wstring(O2EM_VERSION_PATCH) +
        std::wstring(O2emVersion::kAppVersionSuffix,
            O2emVersion::kAppVersionSuffix + strlen(O2emVersion::kAppVersionSuffix));
    return agent.c_str();
}

constexpr DWORD kResolveTimeoutMs = 8000;
constexpr DWORD kConnectTimeoutMs = 10000;
constexpr DWORD kSendTimeoutMs = 10000;
constexpr DWORD kReceiveTimeoutMs = 10000;
// The releases response is a few KB; refuse to buffer anything absurd.
constexpr DWORD kMaxResponseBytes = 1024 * 1024;

// RAII for the three WinHTTP handle levels; early returns stay leak-free.
struct HttpHandle
{
    HINTERNET handle = nullptr;
    ~HttpHandle() { if (handle) WinHttpCloseHandle(handle); }
};

// ---- version parsing / comparison ---------------------------------------
//
// Accepts display forms such as "v0.31.0-beta", "0.32.0-beta", or a bare
// "0.33.0": an optional leading 'v'/'V', then a numeric MAJOR.MINOR.PATCH,
// then an optional simple suffix beginning with '-' or '+' (e.g. "-beta").
// Anything else — garbage, missing components, extra dot-separated parts —
// is REJECTED so a malformed release tag can never silently compare as
// 0.0.0. Returns false for malformed input.
bool ParseVersion(const std::string& version, int outParts[3])
{
    std::string::size_type pos = 0;
    if (pos < version.size() && (version[pos] == 'v' || version[pos] == 'V'))
        ++pos;
    for (int i = 0; i < 3; ++i)
    {
        if (i > 0)
        {
            if (pos >= version.size() || version[pos] != '.')
                return false;
            ++pos;
        }
        if (pos >= version.size() || version[pos] < '0' || version[pos] > '9')
            return false;
        int value = 0;
        while (pos < version.size() && version[pos] >= '0' &&
            version[pos] <= '9')
        {
            value = value * 10 + (version[pos] - '0');
            if (value > 99999999)
                value = 99999999; // saturate; numeric parts never overflow
            ++pos;
        }
        outParts[i] = value;
    }
    // Optional simple suffix ("", "-beta", "+build"); nothing else allowed.
    return pos >= version.size() || version[pos] == '-' ||
        version[pos] == '+';
}

// Three-way compare of parsed MAJOR.MINOR.PATCH; the suffix is ignored.
int CompareVersionParts(const int a[3], const int b[3])
{
    for (int i = 0; i < 3; ++i)
    {
        if (a[i] != b[i])
            return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

// ---- minimal JSON field extraction --------------------------------------
//
// The releases response is a small JSON document; this extracts the value
// of the first occurrence of a simple string field ("tag_name":"...").
// Only flat "key":"value" pairs are recognized — enough for tag_name, no
// JSON framework. Malformed input yields an empty string.
std::string ExtractFirstStringField(
    const std::string& json, const char* field)
{
    const std::string needle = std::string("\"") + field + "\"";
    std::string::size_type pos = json.find(needle);
    while (pos != std::string::npos)
    {
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos)
            return {};
        ++pos;
        while (pos < json.size() &&
            (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' ||
                json[pos] == '\n'))
            ++pos;
        if (pos < json.size() && json[pos] == '"')
        {
            std::string value;
            ++pos;
            while (pos < json.size() && json[pos] != '"')
            {
                if (json[pos] == '\\' && pos + 1 < json.size())
                {
                    // Minimal escape handling: keep the next raw character.
                    value += json[pos + 1];
                    pos += 2;
                    continue;
                }
                value += json[pos++];
            }
            return value;
        }
        pos = json.find(needle, pos); // not a string value; keep searching
    }
    return {};
}

// "v0.32.0-beta" | "0.32.0-beta" | "0.32.0" -> "v0.32.0-beta"
std::string NormalizeDisplayTag(std::string tag)
{
    if (tag.empty())
        return {};
    if (tag[0] == 'v' || tag[0] == 'V')
        tag[0] = 'v';
    else
        tag.insert(tag.begin(), 'v');
    return tag;
}

std::string WideToNarrow(const wchar_t* wide)
{
    if (!wide || !*wide)
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1,
        nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string narrow(static_cast<std::string::size_type>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, narrow.data(),
        size, nullptr, nullptr);
    return narrow;
}

} // namespace

// ---- WinHTTP ------------------------------------------------------------

bool UpdateCheck::HttpsGet(const wchar_t* host, const wchar_t* path,
    std::string& outBody, std::string& outError)
{
    outBody.clear();
    outError.clear();

    HttpHandle session{WinHttpOpen(BuildUserAgent(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.handle)
    {
        outError = "Could not initialize networking";
        return false;
    }
    WinHttpSetTimeouts(session.handle, kResolveTimeoutMs, kConnectTimeoutMs,
        kSendTimeoutMs, kReceiveTimeoutMs);

    HttpHandle connect{WinHttpConnect(session.handle, host,
        INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connect.handle)
    {
        outError = "Could not reach GitHub";
        return false;
    }

    HttpHandle request{WinHttpOpenRequest(connect.handle, L"GET", path,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE)};
    if (!request.handle)
    {
        outError = "Could not create request";
        return false;
    }

    if (!WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS,
            0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
    {
        outError = "Could not send request";
        return false;
    }
    if (!WinHttpReceiveResponse(request.handle, nullptr))
    {
        outError = "GitHub did not respond";
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request.handle,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX))
    {
        outError = "Could not read response status";
        return false;
    }
    if (statusCode != 200)
    {
        outError = "GitHub returned HTTP error " + std::to_string(statusCode);
        return false;
    }

    // Read the body in chunks, honoring shutdown between chunks.
    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_)
            {
                outError.clear();
                return false;
            }
        }
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.handle, &available))
        {
            outError = "Connection lost while reading response";
            return false;
        }
        if (available == 0)
            break;
        if (outBody.size() + available > kMaxResponseBytes)
        {
            outError = "Unexpected response size";
            return false;
        }
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request.handle, chunk.data(), available, &read))
        {
            outError = "Connection lost while reading response";
            return false;
        }
        chunk.resize(read);
        outBody += chunk;
    }
    if (outBody.empty())
    {
        outError = "Empty response from GitHub";
        return false;
    }
    return true;
}

// ---- worker -------------------------------------------------------------

void UpdateCheck::WorkerMain(UpdateCheck* self,
    std::string currentVersion)
{
    std::string body;
    std::string error;
    const bool ok = self->HttpsGet(kApiHost, kReleasesPath, body, error);

    Result finished;
    if (ok)
    {
        // Published, newest-first. Unauthenticated responses never include
        // draft releases, so [0] is the newest PUBLIC release. A release
        // marked prerelease=true still appears here (that is how the current
        // v0.31.0-beta is published); only /releases/latest filters those.
        const std::string tag = ExtractFirstStringField(body, "tag_name");
        int latestParts[3] = {0, 0, 0};
        int currentParts[3] = {0, 0, 0};
        if (tag.empty())
        {
            error = "Unexpected GitHub response";
        }
        else if (!ParseVersion(tag, latestParts))
        {
            // A malformed tag must read as a failed check, never as
            // "up to date" against a silently-parsed 0.0.0.
            error = "Unexpected GitHub version format";
        }
        else if (!ParseVersion(currentVersion, currentParts))
        {
            error = "Unexpected application version";
        }
        else
        {
            const std::string latest = NormalizeDisplayTag(tag);
            if (CompareVersionParts(latestParts, currentParts) > 0)
            {
                finished.state = State::UpdateAvailable;
                finished.latestVersion = latest;
                finished.releaseUrl = WideToNarrow(kRepoPageBase) + tag;
            }
            else
            {
                finished.state = State::UpToDate;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        if (self->stop_)
            return; // app is shutting down; drop the result
        if (!ok)
            finished.state = State::Error;
        finished.errorMessage = error;
        self->result_ = std::move(finished);
        self->busy_ = false;
    }
}

// ---- public API ----------------------------------------------------------

UpdateCheck::~UpdateCheck()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    if (worker_.joinable())
        worker_.join();
}

void UpdateCheck::StartCheck(const std::string& currentVersion)
{
    std::thread finished;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_ || busy_)
            return;
        // A previous check may have completed without being joined yet:
        // WorkerMain clears busy_ under the mutex but leaves worker_
        // joinable. Reap it here before reusing the thread object; that
        // is what previously made thread assignment call std::terminate().
        // busy_ is false, so the reaped worker is finished and never needs
        // mutex_ again; the join below happens after the lock is released,
        // so no mutex is ever held while joining.
        if (worker_.joinable())
            finished = std::move(worker_);
        busy_ = true;
        result_.state = State::Checking;
        result_.latestVersion.clear();
        result_.releaseUrl.clear();
        result_.errorMessage.clear();
        worker_ = std::thread(&UpdateCheck::WorkerMain, this, currentVersion);
    }
    // The reaped worker is already finished, so this returns immediately;
    // it runs outside the mutex and cannot block a running worker.
    if (finished.joinable())
        finished.join();
}

UpdateCheck::State UpdateCheck::GetState() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return result_.state;
}

std::string UpdateCheck::GetLatestVersion() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return result_.latestVersion;
}

std::string UpdateCheck::GetReleaseUrl() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return result_.releaseUrl;
}

std::string UpdateCheck::GetErrorMessage() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return result_.errorMessage;
}
