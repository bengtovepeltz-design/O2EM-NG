//
// src/update_check.h
//
// Phase 2 (check-for-updates only): queries the newest PUBLIC GitHub
// release of O2EM-NG and compares it against the compiled-in application
// version from src/version.h.
//
// LOOK, COMPARE, REPORT — nothing more. This module never downloads,
// stages, installs or replaces anything.
//
// One instance owns at most one background worker thread. StartCheck()
// is ignored while a check is running. Results are handed back through
// a mutex-protected snapshot polled by the UI thread; the worker never
// touches SDL or the frontend.
//
#pragma once

#include <mutex>
#include <string>
#include <thread>

class UpdateCheck
{
public:
    enum class State
    {
        Idle,          // no check started yet
        Checking,      // worker running
        UpToDate,      // newest public release <= current version
        UpdateAvailable, // newest public release > current version
        Error          // network/HTTP/parse failure (details in ErrorMessage)
    };

    UpdateCheck() = default;
    ~UpdateCheck();
    UpdateCheck(const UpdateCheck&) = delete;
    UpdateCheck& operator=(const UpdateCheck&) = delete;

    // Starts one background check unless one is already running.
    // currentVersion: display form, e.g. "v0.31.0-beta" (from src/version.h).
    void StartCheck(const std::string& currentVersion);

    // Poll from the UI thread: reads the current snapshot (cheap, mutex).
    State GetState() const;
    // Valid when GetState() == UpdateAvailable; display form with "v",
    // e.g. "v0.32.0-beta". Empty otherwise.
    std::string GetLatestVersion() const;
    // Valid when GetState() == UpdateAvailable; GitHub release page URL.
    std::string GetReleaseUrl() const;
    // Short human-readable failure reason, valid when GetState() == Error.
    std::string GetErrorMessage() const;

private:
    struct Result
    {
        State state = State::Idle;
        std::string latestVersion; // display form, e.g. "v0.32.0-beta"
        std::string releaseUrl;    // https://github.com/... releases page
        std::string errorMessage;  // short reason for State::Error
    };

    static void WorkerMain(UpdateCheck* self, std::string currentVersion);
    // HTTPS GET returning the response body; false + short reason on error.
    // Not static: the read loop honors stop_ for clean shutdown.
    bool HttpsGet(const wchar_t* host, const wchar_t* path,
        std::string& outBody, std::string& outError);

    mutable std::mutex mutex_;
    bool stop_ = false;              // set on destruction; worker exits
    bool busy_ = false;              // a worker is running
    Result result_;                  // last completed/failed check
    std::thread worker_;             // joinable while busy_; a completed
                                     // worker is reaped by the next
                                     // StartCheck() or the destructor
};
