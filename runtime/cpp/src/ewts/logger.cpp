
#include "ewts/logger.hpp"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include <sys/time.h>
#include <time.h>
#include <dlfcn.h>

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
void ewts_ngen_log(const char* ewts_id, int level, const char* message);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
void ewts_ngen_payload_status(
    const char* ewts_id,
    const char* status,
    double prog,
    const char* msg,
    const char* modnm);

namespace ewts {

static constexpr const char* EV_NGEN_RESULTS_DIR = "NGEN_RESULTS_DIR";
static constexpr const char* EV_EWTS_ENABLED     = "EWTS_ENABLED";
static constexpr const char* EV_EWTS_LOG_DIR     = "EWTS_LOG_DIR";
static constexpr const char* EV_EWTS_LOG_LEVEL   = "EWTS_LOG_LEVEL";

static int  g_mpiRank = -1;

using ewts_ngen_log_fn = void(*)(const char*, int, const char*);
using ewts_ngen_payload_status_fn = void(*)(const char*, const char*, double, const char*, const char*);

static std::mutex g_registry_mtx;
static std::unordered_map<std::string, std::unique_ptr<Logger>> g_loggers;
static thread_local Logger* g_current_logger = nullptr;

static bool is_ngen_active() {
    const char* v = std::getenv(EV_NGEN_RESULTS_DIR);
    return (v && *v);
}

static ewts_ngen_log_fn resolve_ngen_log() {
    if (void* sym = dlsym(RTLD_DEFAULT, "ewts_ngen_log")) {
        return reinterpret_cast<ewts_ngen_log_fn>(sym);
    }
    return nullptr;
}

static ewts_ngen_payload_status_fn resolve_ngen_payload_status() {
    if (void* sym = dlsym(RTLD_DEFAULT, "ewts_ngen_payload_status")) {
        return reinterpret_cast<ewts_ngen_payload_status_fn>(sym);
    }
    return nullptr;
}

static std::string trim_upper(std::string s) {
    auto l = s.find_first_not_of(" \t\r\n");
    auto r = s.find_last_not_of(" \t\r\n");
    s = (l == std::string::npos) ? "" : s.substr(l, r - l + 1);
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static bool parse_enabled(const char* v) {
    if (!v || !*v) return true;
    std::string s = trim_upper(v);
    if (s.empty()) return true;
    return !(s == "0" || s == "FALSE" || s == "NO" || s == "OFF" || s == "DISABLED");
}

static LogLevel parse_level(const char* v) {
    if (!v || !*v) return LogLevel::NOTSET;
    std::string s(v);
    auto l = s.find_first_not_of(" \t\r\n");
    auto r = s.find_last_not_of(" \t\r\n");
    s = (l == std::string::npos) ? "" : s.substr(l, r - l + 1);
    if (s.empty()) return LogLevel::NOTSET;

    char* end = nullptr;
    long num = std::strtol(s.c_str(), &end, 10);
    if (end && *end == '\0' && num >= 0) return static_cast<LogLevel>(num);

    s = trim_upper(s);
    if (s == "DEBUG") return LogLevel::DEBUG;
    if (s == "PERFORM") return LogLevel::PERFORM;
    if (s == "INFO") return LogLevel::INFO;
    if (s == "WARN" || s == "WARNING") return LogLevel::WARNING;
    if (s == "ERROR" || s == "SEVERE") return LogLevel::SEVERE;
    if (s == "FATAL" || s == "CRITICAL") return LogLevel::FATAL;
    if (s == "STATUS") return LogLevel::STATUS;
    return LogLevel::NOTSET;
}

static const char* level_name_padded(LogLevel lvl) {
    switch (static_cast<int>(lvl)) {
        case 10: return "DEBUG  ";
        case 15: return "PERFORM";
        case 20: return "INFO   ";
        case 30: return "WARNING";
        case 40: return "SEVERE ";
        case 50: return "FATAL  ";
        case 60: return "STATUS ";
        default: return "NOTSET ";
    }
}

static std::string utc_timestamp_iso_ms() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    tm tm_utc{};
    gmtime_r(&tv.tv_sec, &tm_utc);
    char base[32];
    std::strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &tm_utc);
    unsigned ms3 = static_cast<unsigned>(tv.tv_usec / 1000) % 1000u;
    char out[48];
    int n = std::snprintf(out, sizeof(out), "%s.%03uZ", base, ms3);
    if (n < 0 || static_cast<std::size_t>(n) >= sizeof(out)) {
        return "0000-00-00T00:00:00.000Z";
    }
    return std::string(out);
}

static std::string utc_timestamp_compact() {
    std::time_t t = std::time(nullptr);
    tm tm_utc{};
    gmtime_r(&t, &tm_utc);
    char out[32];
    std::strftime(out, sizeof(out), "%Y%m%dT%H%M%S", &tm_utc);
    return std::string(out);
}

Logger::Logger(std::string ewts_id, bool ewts_ngen)
    : ewts_id_(trim_upper(std::move(ewts_id))), use_ngen_(ewts_ngen) {}

std::string Logger::module_loglevel_env() const {
    return ewts_id_ + "_LOGLEVEL";
}

void Logger::pad_id() {
    if (ewts_id_.size() >= 8) ewts_id_padded_ = ewts_id_.substr(0, 8);
    else ewts_id_padded_ = ewts_id_ + std::string(8 - ewts_id_.size(), ' ');
}

bool Logger::have_ngen_bridge() const {
    static ewts_ngen_log_fn g_ngen_log = resolve_ngen_log();
    return use_ngen_ && is_ngen_active() && (g_ngen_log != nullptr);
}

void Logger::open_standalone_file() {
    if (out_.is_open()) return;

    // EV_EWTS_LOG_DIR is a fallback env var not set by ngen
    // RTE or the user can set this when running standalone
    // to direct where the logs should be written
    const char* dir = std::getenv(EV_EWTS_LOG_DIR);

    // New default: if EWTS_LOG_DIR is not explicitly set,
    // do not open a file. Log() will fall back to std::cout.
    if (!(dir && *dir)) {
        path_.clear();
        return;
    }

    std::string log_dir = dir;

    std::filesystem::create_directories(log_dir);

    path_ = log_dir + "/" + ewts_id_ + "_" + utc_timestamp_compact() + ".log";
    out_.open(path_, std::ios::out | std::ios::app);
}

void Logger::init_once() {
    std::lock_guard<std::mutex> lk(init_mtx_);
    if (initialized_) return;
    initialized_ = true;

    g_mpiRank  = -1;
    const char* r = std::getenv("EWTS_RANK");
    if (r && *r) {
        char* end = nullptr;
        long val = std::strtol(r, &end, 10);
        if (end && *end == '\0' && val >= 0) {
            g_mpiRank = static_cast<int>(val);
        }
    }

    std::string prefix = (g_mpiRank >= 0)
        ? "[rank " + std::to_string(g_mpiRank) + "] EWTS "
        : "EWTS ";

    enabled_ = parse_enabled(std::getenv(EV_EWTS_ENABLED));

    // Build string first to minimize risk of stdout buffer interleaving during mpi runs
    std::ostringstream oss;
    oss << prefix << ewts_id_ << " logging is "<< (enabled_ ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << oss.str() << std::flush;

    auto key = module_loglevel_env();
    auto lvl = parse_level(std::getenv(key.c_str()));

    oss.str("");     // clear the contents
    oss.clear();     // reset stream state flags
    oss << prefix << ewts_id_ << " log level from env var "
        << key << " is " << level_name_padded(lvl) << std::endl;
    std::cout << oss.str() << std::flush;

    if (static_cast<int>(lvl) != 0) level_ = lvl;
    else {
        lvl = parse_level(std::getenv(EV_EWTS_LOG_LEVEL));
        level_ = (static_cast<int>(lvl) != 0) ? lvl : LogLevel::INFO;
    }

    oss.str("");
    oss.clear();
    oss << prefix << ewts_id_ << " log level set to "
        << level_name_padded(level_) << std::endl;
    std::cout << oss.str() << std::flush;

    pad_id();

    if (have_ngen_bridge()) {
        oss.str("");     // clear the contents
        oss.clear();     // reset stream state flags
        oss << prefix << ewts_id_ << " using ngen for logging" << std::endl;
        std::cout << oss.str() << std::flush;
    }
    else
        std::cout << prefix << ewts_id_ << " logging standalone" << std::endl;
}

bool Logger::IsLoggingEnabled() {
    init_once();
    return enabled_;
}

LogLevel Logger::GetLogLevel() {
    init_once();
    return level_;
}

void Logger::Log(LogLevel level, std::string_view message) {
    init_once();
    if (!enabled_) return;
    if (static_cast<int>(level) < static_cast<int>(level_)) return;

    static ewts_ngen_log_fn g_ngen_log = resolve_ngen_log();

    if (use_ngen_ && is_ngen_active() && g_ngen_log) {
        std::string msg(message);
        g_ngen_log(ewts_id_.c_str(), static_cast<int>(level), msg.c_str());
        return;
    }

    const auto ts = utc_timestamp_iso_ms();
    const char* lvl = level_name_padded(level);

    std::lock_guard<std::mutex> lk(log_mtx_);
    open_standalone_file();

    std::ostream& out = out_.is_open() ? out_ : std::cout;
    out << ts << " " << ewts_id_padded_ << " " << lvl << " " << message << "\n";
    out.flush();
}

void Logger::Log(std::string_view message, LogLevel level) {
    Log(level, message);
}

void Logger::Log(LogLevel level, const char* fmt, ...) {
    if (!fmt) return;

    va_list ap;
    va_start(ap, fmt);

    va_list ap2;
    va_copy(ap2, ap);
    int n = std::vsnprintf(nullptr, 0, fmt, ap2);
    va_end(ap2);

    if (n < 0) {
        va_end(ap);
        return;
    }

    std::string buf(static_cast<std::size_t>(n), '\0');
    std::vsnprintf(buf.data(), static_cast<std::size_t>(n) + 1, fmt, ap);
    va_end(ap);

    Log(level, buf);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
// API convenience wrappers
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
Logger& GetLogger(std::string_view ewts_id, bool ewts_ngen) {
    std::string key = trim_upper(std::string(ewts_id.empty() ? "EWTS" : ewts_id));

    std::lock_guard<std::mutex> lk(g_registry_mtx);
    auto it = g_loggers.find(key);
    if (it != g_loggers.end()) return *(it->second);

    auto ptr = std::make_unique<Logger>(key, ewts_ngen);
    Logger& ref = *ptr;
    g_loggers.emplace(key, std::move(ptr));
    return ref;
}

Logger& CurrentLogger() {
    if (g_current_logger != nullptr) {
        return *g_current_logger;
    }
    return GetLogger("EWTS");
}

void EwtsInit(std::string_view ewts_id, bool ewts_ngen) {
    g_current_logger = &GetLogger(ewts_id, ewts_ngen);
}

bool IsLoggingEnabled() {
    return CurrentLogger().IsLoggingEnabled();
}

LogLevel GetLogLevel() {
    return CurrentLogger().GetLogLevel();
}

void Log(LogLevel level, std::string_view message) {
    CurrentLogger().Log(level, message);
}

void Log(std::string_view message, LogLevel level) {
    CurrentLogger().Log(level, message);
}

void PayloadStatus(
    const char* ewts_id,
    const char* status,
    double prog,
    const char* msg,
    const char* modnm)
{
    static ewts_ngen_payload_status_fn g_payload_status =
        resolve_ngen_payload_status();

    if (is_ngen_active() && g_payload_status) {
        g_payload_status(
            ewts_id ? ewts_id : "",
            status ? status : "",
            prog,
            msg ? msg : "",
            modnm ? modnm : "");
    }
}
}
