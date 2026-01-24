#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

class Logger {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void SetTimestamp(TimePoint ts) {
        std::lock_guard<std::mutex> lk(mutex_);
        forced_ts_ = ts;
    }

    void ResetTimestamp() {
        std::lock_guard<std::mutex> lk(mutex_);
        forced_ts_.reset();
    }


    template <class... Args>
    void Log(Args&&... args) {

        std::lock_guard<std::mutex> lk(mutex_);

        const TimePoint tp = GetNowLocked_();
        const auto date_key = MakeDateKey_(tp);  
        RotateFileIfNeededLocked_(date_key);

        out_ << GetTimeStamp_(tp) << ": ";

        (out_ << ... << std::forward<Args>(args));

        out_ << '\n';
        out_.flush();
    }

private:
    Logger() = default;
    ~Logger() = default;

private:
    TimePoint GetNowLocked_() const {
        if (forced_ts_) {
            return *forced_ts_;
        }
        return Clock::now();
    }

    static std::tm ToLocalTm_(std::time_t t) {
        std::tm tm{};
        localtime_r(&t, &tm);
        return tm;
    }

    static std::string GetTimeStamp_(TimePoint tp) {
        const std::time_t t = Clock::to_time_t(tp);
        const std::tm tm = ToLocalTm_(t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    static std::string MakeDateKey_(TimePoint tp) {
        const std::time_t t = Clock::to_time_t(tp);
        const std::tm tm = ToLocalTm_(t);

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << (tm.tm_year + 1900) << '_'
            << std::setw(2) << (tm.tm_mon + 1) << '_'
            << std::setw(2) << tm.tm_mday;
        return oss.str();
    }

    static std::string MakeFilePath_(const std::string& date_key) {
        // date_key уже YYYY_MM_DD
        return "/var/log/sample_log_" + date_key + ".log";
    }

    void RotateFileIfNeededLocked_(const std::string& date_key) {
        if (current_date_key_ == date_key && out_.is_open()) {
            return;
        }

        current_date_key_ = date_key;

        if (out_.is_open()) {
            out_.close();
        }

        const std::string path = MakeFilePath_(date_key);
        out_.open(path, std::ios::out | std::ios::app);
    }

private:
    mutable std::mutex mutex_;
    std::optional<TimePoint> forced_ts_;

    std::string current_date_key_;
    std::ofstream out_;
};
