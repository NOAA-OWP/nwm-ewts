#include "ewts_ngen/logger.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mpi.h>
#include <map>
#include <mutex>
#include <sstream>
#include <sys/stat.h>

#define BOOST_BIND_GLOBAL_PLACEHOLDERS // intentionally want the old behavior
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "ewts_ngen/ngen_module_keys.hpp"
#include "ewts/log_levels.hpp"

int Logger::g_mpiRank = 0;

namespace {

static const char* const kEnvResultsDir    = "NGEN_RESULTS_DIR";
static const char* const kEnvLogfilePrefix = "NGEN_LOG_FILE_PREFIX";
static const char* const kEnvUseNgenBridge = "EWTS_USE_NGEN_BRIDGE";
static const char* const kEnvEwtsLogDir    = "EWTS_LOG_DIR";
static const char* const kConfigFilename   = "ngen_logging.json";
static const char* const kEnvEwtsEnabled   = "EWTS_ENABLED";
static const char* const kEnvEwtsLogLevel  = "EWTS_LOG_LEVEL";
static std::string       kLogRankLabel     = "mpi_process";

inline bool IsDigitString(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

inline std::string ToUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

inline std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

inline std::string TrimString(const std::string& str) {
    const char* ws = " \t\n\r\f\v";
    const size_t first = str.find_first_not_of(ws);
    if (first == std::string::npos) return "";
    const size_t last = str.find_last_not_of(ws);
    return str.substr(first, last - first + 1);
}

inline LogLevel ClampCanonicalLevel(int v) {
    // Canonical allowed numeric levels: 10/20/30/40/50 (and 0).
    if (v <= 0)  return LogLevel::NOTSET;
    if (v <= 10) return LogLevel::DEBUG;
    if (v <= 15) return LogLevel::PERFORM;
    if (v <= 20) return LogLevel::INFO;
    if (v <= 30) return LogLevel::WARNING;
    if (v <= 40) return LogLevel::SEVERE;
    if (v <= 50) return LogLevel::FATAL;
    return LogLevel::STATUS;
}

inline LogLevel ParseLevel(const std::string& value) {
    std::string v = ToLower(TrimString(value));
    if (v == "debug") return LogLevel::DEBUG;
    if (v == "performance" || v == "perform") return LogLevel::PERFORM;
    if (v == "info") return LogLevel::INFO;
    if (v == "warning" || v == "warn") return LogLevel::WARNING;
    if (v == "error" || v == "severe") return LogLevel::SEVERE;
    if (v == "fatal" || v == "critical") return LogLevel::FATAL;
    if (v == "status") return LogLevel::STATUS;
    if (v == "notset" || v == "none") return LogLevel::NOTSET;
    // Also accept "10"/"20"... (handled earlier), but in case:
    if (IsDigitString(v)) return ClampCanonicalLevel(std::atoi(v.c_str()));
    return LogLevel::INFO;
}

constexpr std::size_t EWTS_ID_WIDTH = 8;

inline std::string PadEwtsId(const std::string& id) {
    std::string s = ToUpper(id);
    if (s.size() >= EWTS_ID_WIDTH) return s.substr(0, EWTS_ID_WIDTH);
    s.append(EWTS_ID_WIDTH - s.size(), ' ');
    return s;
}

inline bool mpi_is_initialized() {
    int flag = 0;
    MPI_Initialized(&flag);
    return flag != 0;
}

inline bool should_log_line(std::string& s) {
    if (!s.empty() && s.back() == '\r') {
        s.pop_back();
    }
    return s.find_first_not_of(" \t") != std::string::npos;
}

inline LogLevel get_default_log_level() {
    const char* lvl = std::getenv(kEnvEwtsLogLevel);
    if (lvl && std::strlen(lvl) > 0) {
        // Build string first to minimize risk of stdout buffer interleaving during mpi runs
        std::ostringstream oss;
        if (Logger::g_mpiRank >= 0) oss << "[rank " << Logger::g_mpiRank <<  "] ";
        oss << "EWTS NGEN Found env var " << kEnvEwtsLogLevel << ". Default log level set to " << lvl << '\n';
        std::cout << oss.str() << std::flush;
        std::string val = TrimString(lvl);

        if (IsDigitString(val)) {
            return ClampCanonicalLevel(std::atoi(val.c_str()));
        } else {
            return ParseLevel(val);
        }
    }
    // Build string first to minimize risk of stdout buffer interleaving during mpi runs
    std::ostringstream oss;
    if (Logger::g_mpiRank >= 0) oss << "[rank " << Logger::g_mpiRank <<  "] ";
    oss << "EWTS NGEN env var " << kEnvEwtsLogLevel << " not found. Using fallback default log level INFO\n";
    std::cout << oss.str() << std::flush;

    return LogLevel::INFO;
}

inline std::string GetLogFilePrefixFromEnv() {
    const char* p = std::getenv(kEnvLogfilePrefix);
    if (!p || std::strlen(p) == 0) {
        return "";
    }

    const std::string prefix = TrimString(std::string(p));
    return prefix;
}

inline std::string PrependLogFilePrefix(const std::string& baseName) {
    const std::string prefix = GetLogFilePrefixFromEnv();
    if (prefix.empty()) {
        return baseName;
    }
    return prefix + "_" + baseName;
}

static constexpr const char* kPayloadBeginSentinel = "<MSG_DATA>";
static constexpr const char* kPayloadEndSentinel = "</MSG_DATA>";

struct PayloadExtractResult {
    bool ok = false;
    std::string json;
    std::string error_msg;
};

inline std::string MissingSentinelMessage(const char* sentinel)
{
    return std::string("Malformed payload: missing sentinel ") + sentinel;
}

inline PayloadExtractResult ExtractJsonPayload(const char* message)
{
    PayloadExtractResult result;

    if (!message) {
        result.error_msg = "Payload message is null";
        return result;
    }

    const std::string s(message);

    const bool has_begin = s.find(kPayloadBeginSentinel) != std::string::npos;
    const bool has_end   = s.find(kPayloadEndSentinel) != std::string::npos;

    // No sentinals found
    if (!has_begin && !has_end) {
        result.ok = true;
        result.json = s;
        return result;
    }

    if (has_begin && !has_end) {
        result.error_msg = MissingSentinelMessage(kPayloadEndSentinel);
        return result;
    }

    if (!has_begin && has_end) {
        result.error_msg = MissingSentinelMessage(kPayloadBeginSentinel);
        return result;
    }

    const std::size_t begin_pos = s.find(kPayloadBeginSentinel);
    const std::size_t json_start = begin_pos + std::strlen(kPayloadBeginSentinel);
    const std::size_t end_pos = s.find(kPayloadEndSentinel, json_start);

    if (end_pos == std::string::npos) {
        result.error_msg = MissingSentinelMessage(kPayloadEndSentinel);
        return result;
    }

    result.ok = true;
    result.json = s.substr(json_start, end_pos - json_start);
    return result;
}

inline std::string EscapeJson(const std::string& s)
{
    std::string out;

    for (char c : s)
    {
        switch (c)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:   out += c;
        }
    }

    return out;
}

} // namespace

Logger* Logger::GetLogger() {
    static Logger instance;   // C++11: initialized once, thread-safe
    return &instance;
}

void Logger::InitIfNeeded() {
    static std::once_flag once;
    std::call_once(once, [this]() {

        std::ostringstream oss;

        // Ensure all logging environment variables controlled by ngen are not yet set
        ConfigureEnvVars(false);

        // Determine MPI rank (optional)
        if (mpi_is_initialized()) {
            int r = 0;
            MPI_Comm_rank(MPI_COMM_WORLD, &r);
            g_mpiRank = r;

            std::string val = std::to_string(g_mpiRank);
            setenv("EWTS_RANK", val.c_str(), 1);

            // Build string first to minimize risk of stdout buffer interleaving during mpi runs
            oss.str("");     // clear the contents
            oss.clear();     // reset stream state flags
            oss << "[rank " << g_mpiRank << "] EWTS NGEN Running with MPI\n";
            std::cout << oss.str() << std::flush;

            oss.str("");     // clear the contents
            oss.clear();     // reset stream state flags
            oss << "[rank " << g_mpiRank << "] EWTS NGEN env var EWTS_RANK set to " << g_mpiRank << '\n';
            std::cout << oss.str() << std::flush;
        } else {
            g_mpiRank = -1;
            std::cout << "EWTS NGEN Running WITHOUT MPI\n"  << std::flush;
        }

        std::string ngenResultsDir;

        // Determine results dir
        oss.str("");     // clear the contents
        oss.clear();     // reset stream state flags
        const char* rd = std::getenv(kEnvResultsDir);
        if (rd && std::strlen(rd) > 0) {
            ngenResultsDir = std::string(rd);
            oss << "EWTS NGEN Found env var " << kEnvResultsDir << " = " << ngenResultsDir << '\n';
        }
        else {
            oss << "EWTS NGEN env var " << kEnvResultsDir << " NOT FOUND" << '\n';
        }
        std::cout << oss.str() << std::flush;


        // Determine module EWTS id (for log message prefix)
        // moduleKey is stable key (lowercase)
        const char* id_c = ewts_ngen::EwtsIdFromKey(moduleKey.c_str());
        if (id_c) {
            ewtsId = std::string(id_c);
        }

        // Read config only when NGEN_RESULTS_DIR is set, per requirements.
        bool loaded = false;
        if (!ngenResultsDir.empty()) {
            loaded = ReadConfigFromResultsDir(ngenResultsDir);
        }

        // Determine default log level based on env var or fallback default level
        LogLevel defaultLogLevel = get_default_log_level();
        if (!loaded) {
            // Defaults when no results dir
            loggingEnabled = true;
            splitLogsByModule = false;
            // Prepopulate defaults for known modules (INFO) when generated registry is available.
            moduleLogLevels.clear();
            for (const auto& e : ewts_ngen::kModules) {
                if (e.key && *e.key) {                 // non-null and not ""
                    moduleLogLevels[std::string(e.key)] = defaultLogLevel;
                }
            }
            moduleLogLevels[moduleKey] = defaultLogLevel;
            logLevel = defaultLogLevel;
        }

        ConfigureEnvVars(true);

        SetupLogFile(ngenResultsDir);
    });
}

std::string Logger::GetParentDirName(const std::string& path) {
    if (path.empty()) return "";

    const std::size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return "";
    if (pos == 0) return "/";

    return path.substr(0, pos);
}

bool Logger::FindConfigFileFromPath(std::string path, std::string& configPath) {
    while (!path.empty()) {
        const std::string candidate = JoinPath(path, kConfigFilename);
        if (FileExists(candidate)) {
            configPath = candidate;
            return true;
        }

        const std::string parent = GetParentDirName(path);
        if (parent.empty() || parent == path) break;
        path = parent;
    }

    return false;
}

bool Logger::ReadConfigFromResultsDir(const std::string& resultsDir) {
    // Defaults
    loggingEnabled = true;
    splitLogsByModule = false;
    moduleLogLevels[moduleKey] = LogLevel::INFO;

    std::string cfg;
    if (!FindConfigFileFromPath(resultsDir, cfg)) {
        // Build string first to minimize risk of stdout buffer interleaving during mpi runs
        std::ostringstream oss;
        if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
        oss << "WARNING: EWTS config file " << kConfigFilename
            << " NOT FOUND in " << resultsDir
            << " or any parent directory. Defaults will be used\n";
        std::cout << oss.str() << std::flush;
        // No config file: keep defaults, but still export log level environment variables.
        logLevel = moduleLogLevels[moduleKey];
        return false;
    }
    // Build string first to minimize risk of stdout buffer interleaving during mpi runs
    std::ostringstream oss;
    if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
    oss << "EWTS NGEN config file " << cfg << '\n';
    std::cout << oss.str() << std::flush;

    boost::property_tree::ptree pt;
    try {
        boost::property_tree::read_json(cfg, pt);
    } catch (const std::exception& e) {
        logLevel = moduleLogLevels[moduleKey];
        // If config is malformed, fall back to defaults but keep logging enabled.
        std::cerr << "WARNING: failed to parse " << cfg << ": " << e.what() << std::endl;
        return false;
    }

    // EWTS/logging enabled
    bool enabled = true;
    auto opt_ewts_enabled = pt.get_optional<bool>("ewts_enabled");
    if (opt_ewts_enabled) {
        enabled = *opt_ewts_enabled;
    } else {
        auto opt_logging_enabled = pt.get_optional<bool>("logging_enabled");
        if (opt_logging_enabled) enabled = *opt_logging_enabled;
    }
    loggingEnabled = enabled;
    
    // Build string first to minimize risk of stdout buffer interleaving during mpi runs
    oss.str("");     // clear the contents
    oss.clear();     // reset stream state flags
    if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
    oss << "EWTS NGEN logging " << (loggingEnabled ? "ENABLED" : "DISABLED") << '\n';
    std::cout << oss.str() << std::flush;

    // split_logs_by_module
    auto opt_split = pt.get_optional<bool>("split_logs_by_module");
    if (opt_split) splitLogsByModule = *opt_split;
    oss.str("");     // clear the contents
    oss.clear();     // reset stream state flags
    if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
    oss << "EWTS NGEN logging to "
        << (splitLogsByModule ? "<MODULE>" : "a UNIFIED")
        << " per rank file\n";
    std::cout << oss.str() << std::flush;

    auto modules_child = pt.get_child_optional("modules");
    if (modules_child) {
        for (const auto& kv : *modules_child) {
            const std::string key = TrimString(kv.first);
            const std::string raw = TrimString(kv.second.get_value<std::string>());
            oss.str("");     // clear the contents
            oss.clear();     // reset stream state flags
            if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
            oss << "EWTS NGEN " << key << " log level read from config " << ToUpper(raw) << '\n';
            std::cout << oss.str() << std::flush;

            LogLevel lvl = LogLevel::INFO;
            if (IsDigitString(raw)) {
                lvl = ClampCanonicalLevel(std::atoi(raw.c_str()));
            } else {
                lvl = ParseLevel(raw);
            }
            moduleLogLevels[key] = lvl;
        }
    }

    auto it = moduleLogLevels.find(moduleKey);
    logLevel = (it != moduleLogLevels.end()) ? it->second : get_default_log_level();
    return true;
}

void Logger::ConfigureEnvVars(bool set) {
    if (set) {
        // EWTS_USE_NGEN_BRIDGE enables logging through the ngen bridge when set to "1"
        ::setenv(kEnvUseNgenBridge, "1", 1);

        // EWTS_ENABLED=0|1 (default 1)
        ::setenv(kEnvEwtsEnabled, loggingEnabled ? "1" : "0", 1);

        // <MODULE>_LOGLEVEL=<10|15|20|30|40|50>
        for (const auto& kv : moduleLogLevels) {
            const std::string mod_key = kv.first;
            const LogLevel lvl = kv.second;
            const std::string ident = EnvVarIdentFromModuleKey(mod_key);
            if (ident.empty()) continue;

            const std::string env_name = ident + "_LOGLEVEL";
            const std::string env_val  = std::to_string(static_cast<int>(lvl));
            ::setenv(env_name.c_str(), env_val.c_str(), 1);
            std::ostringstream oss;
            if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
            oss << "EWTS NGEN env var " << env_name << " set to " << LevelToFixedString(lvl) << '\n';
            std::cout << oss.str() << std::flush;
        }
    } else {
        ::unsetenv(kEnvUseNgenBridge);
        ::unsetenv(kEnvEwtsEnabled);

        for (const auto& kv : moduleLogLevels) {
            const std::string ident = EnvVarIdentFromModuleKey(kv.first);
            if (ident.empty()) continue;

            const std::string env_name = ident + "_LOGLEVEL";
            ::unsetenv(env_name.c_str());
            std::ostringstream oss;
            if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank << "] ";
            oss << "EWTS NGEN env var " << env_name << " unset\n";
            std::cout << oss.str() << std::flush;
        }
    }
}

std::string Logger::GetStandaloneBaseDir() {
    const char* ewts_log_dir = std::getenv(kEnvEwtsLogDir);
    if (ewts_log_dir && std::strlen(ewts_log_dir) > 0) {
        return std::string(ewts_log_dir);
    }

    return "";
}

void Logger::SetupLogFile(const std::string& resultsDir) {
    std::ostringstream oss;

    // Determine output directory
    if (!resultsDir.empty()) {
        logFileDir = resultsDir;
    } else {
        logFileDir = GetStandaloneBaseDir();
    }

    if (logFileDir.empty()) {
        logFilePath.clear();
        std::cout << "EWTS NGEN using stdout logging\n" << std::flush;
        return;
    }

    (void)CreateDirectory(logFileDir);

    if (splitLogsByModule) {
        // In split mode, the destination file is selected per log call from the
        // incoming EWTS id, so only the directory is initialized here.
        logFilePath.clear();

        // Build string first to minimize risk of stdout buffer interleaving during mpi runs
        oss.str("");     // clear the contents
        oss.clear();     // reset stream state flags
        if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
        oss << "EWTS NGEN split log files under " << logFileDir << '\n';
        std::cout << oss.str() << std::flush;
        return;
    }

    // Determine file name
    std::string stem = "ngen";

    oss.str("");     // clear the contents
    oss.clear();     // reset stream state flags
    const std::string logPrefix = GetLogFilePrefixFromEnv();
    if (!logPrefix.empty()) {
        oss << "EWTS NGEN Found env var " << kEnvLogfilePrefix << " = " << logPrefix << '\n';
        stem = PrependLogFilePrefix(stem);
    }
    else {
        oss << "EWTS NGEN env var " << kEnvLogfilePrefix << " NOT FOUND\n";
    }
    std::cout << oss.str() << std::flush;

    std::string rank_part;
    if (mpi_is_initialized()) {
        rank_part = "_" + kLogRankLabel + "_" + std::to_string(GetLogger()->GetRank());
    }

    // Optional timestamp suffix (only when no results dir)
    std::string ts_part;
    if (resultsDir.empty()) {
        ts_part = "_" + CreateCompactTimestampUTC();
    }

    const std::string filename = stem + rank_part + ts_part + ".log";
    logFilePath = JoinPath(logFileDir, filename);

    // Open file in truncate mode. The calibration job runs ngen
    // iteratively, and only the logs from the most recent ngen
    // run should be retained.
    logFile.open(logFilePath.c_str(), std::ios::out | std::ios::trunc);

    // Build string to minimize risk of buffer interleaving during mpi runs
    oss.str("");     // clear the contents
    oss.clear();     // reset stream state flags
    if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
    oss << "EWTS NGEN log file " << logFilePath << '\n';
    std::cout << oss.str() << std::flush;
}

bool Logger::LogFileReady() const {
    return logFile.is_open() && logFile.good();
}

void Logger::Log(LogLevel messageLevel, const std::string& message) {
    Logger* logger = GetLogger();
    Log(logger->ewtsId, messageLevel, message);
}

void Logger::Log(const std::string& moduleName, LogLevel messageLevel, const char* message, ...) {
    if (!message) return;

    Logger* logger = GetLogger();
    logger->InitIfNeeded();
    if (!logger->loggingEnabled) return;

    // For bridged/per-module logging, filter using the effective level for the
    // incoming moduleName, not the singleton logger instance's own module level.
    LogLevel effectiveLevel = logger->logLevel;
    bool foundEffectiveLevel = false;
    for (const auto& e : ewts_ngen::kModules) {
        if (e.ewts_id && moduleName == e.ewts_id) {
            if (e.key && *e.key) {
                auto it = logger->moduleLogLevels.find(std::string(e.key));
                if (it != logger->moduleLogLevels.end()) {
                    effectiveLevel = it->second;
                    foundEffectiveLevel = true;
                }
            }
            break;
        }
    }
    if (!foundEffectiveLevel && moduleName == logger->ewtsId) {
        effectiveLevel = logger->logLevel;
        foundEffectiveLevel = true;
    }
    if (!foundEffectiveLevel) {
        effectiveLevel = LogLevel::INFO;
    }
    if (static_cast<int>(messageLevel) < static_cast<int>(effectiveLevel)) return;

    // Format varargs into a std::string
    va_list args1;
    va_start(args1, message);
    va_list args2;
    va_copy(args2, args1);

    const int needed = std::vsnprintf(nullptr, 0, message, args1);
    va_end(args1);
    if (needed < 0) {
        va_end(args2);
        return;
    }

    std::string buf(static_cast<std::size_t>(needed) + 1U, '\0');
    std::vsnprintf(&buf[0], buf.size(), message, args2);
    va_end(args2);

    // remove trailing null
    if (!buf.empty() && buf.back() == '\0') buf.pop_back();

    Log(moduleName, messageLevel, buf);
}

void Logger::Log(LogLevel messageLevel, const char* message, ...) {
    if (!message) return;

    Logger* logger = GetLogger();
    logger->InitIfNeeded();
    if (!logger->loggingEnabled) return;
    if (static_cast<int>(messageLevel) < static_cast<int>(logger->logLevel)) return;

    // Format varargs into a std::string
    va_list args1;
    va_start(args1, message);
    va_list args2;
    va_copy(args2, args1);

    const int needed = std::vsnprintf(nullptr, 0, message, args1);
    va_end(args1);
    if (needed < 0) {
        va_end(args2);
        return;
    }

    std::string buf(static_cast<std::size_t>(needed) + 1U, '\0');
    std::vsnprintf(&buf[0], buf.size(), message, args2);
    va_end(args2);

    // remove trailing null
    if (!buf.empty() && buf.back() == '\0') buf.pop_back();

    Log(logger->ewtsId, messageLevel, buf);
}

void Logger::Log(const std::string& moduleName, LogLevel messageLevel, const std::string& message) {
    Logger* logger = GetLogger();
    logger->InitIfNeeded();

    if (!logger->loggingEnabled) return;

    // Check for Payload status message
    if (messageLevel == LogLevel::STATUS) {
        LogPayload(moduleName.c_str(), message.c_str());
        return;
    }

    // For bridged/per-module logging, filter using the effective level for the
    // incoming moduleName, not the singleton logger instance's own module level.
    LogLevel effectiveLevel = logger->logLevel;
    bool foundEffectiveLevel = false;
    for (const auto& e : ewts_ngen::kModules) {
        if (e.ewts_id && moduleName == e.ewts_id) {
            if (e.key && *e.key) {
                auto it = logger->moduleLogLevels.find(std::string(e.key));
                if (it != logger->moduleLogLevels.end()) {
                    effectiveLevel = it->second;
                    foundEffectiveLevel = true;
                }
            }
            break;
        }
    }
    if (!foundEffectiveLevel && moduleName == logger->ewtsId) {
        effectiveLevel = logger->logLevel;
        foundEffectiveLevel = true;
    }
    if (!foundEffectiveLevel) {
        effectiveLevel = LogLevel::INFO;
    }
    if (static_cast<int>(messageLevel) < static_cast<int>(effectiveLevel)) return;

    const std::string level_str = LevelToFixedString(messageLevel);

    // Prefix: <ISO timestamp> <EWTS_ID padded> <LEVEL padded>
    const std::string prefix = CreateTimestamp(true, true) + " " +
                               PadEwtsId(moduleName) + " " + level_str;

    std::istringstream in(message);
    std::string line;

    if (logger->splitLogsByModule) {
        static std::mutex splitLogMutex;
        static std::map<std::string, std::ofstream> splitLogFiles;

        // Route the message to the file for the EWTS id passed in this log call.
        std::lock_guard<std::mutex> lock(splitLogMutex);

        auto it = splitLogFiles.find(moduleName);
        if (it == splitLogFiles.end()) {
            std::string rank_part;
            if (mpi_is_initialized()) {
                rank_part = "_" + kLogRankLabel + "_" + std::to_string(logger->GetRank());
            }

            // Optional timestamp suffix (only when no results dir)
            std::string ts_part;
            const char* rd = std::getenv(kEnvResultsDir);
            if (!(rd && std::strlen(rd) > 0)) {
                ts_part = "_" + CreateCompactTimestampUTC();
            }

            std::string stem = ToLower(moduleName);
            stem = PrependLogFilePrefix(stem);

            const std::string filename = stem + rank_part + ts_part + ".log";
            const std::string path = JoinPath(logger->logFileDir, filename);

            std::ofstream& splitFile = splitLogFiles[moduleName];
            splitFile.open(path.c_str(), std::ios::out | std::ios::trunc);

            // Build string first to minimize risk of stdout buffer interleaving during mpi runs
            std::ostringstream oss;
            if (g_mpiRank >= 0) oss << "[rank " << g_mpiRank <<  "] ";
            oss << "EWTS NGEN split log file " << path << '\n';
            std::cout << oss.str() << std::flush;
            it = splitLogFiles.find(moduleName);
        }

        if (it != splitLogFiles.end() && it->second.is_open() && it->second.good()) {
            while (std::getline(in, line)) {
                // Ensures empty lines are not logged
                if (!should_log_line(line)) {
                    continue;
                }
                it->second << prefix << " " << line << std::endl;
            }
            it->second.flush();
            return;
        }
    }

    if (logger->LogFileReady()) {
        while (std::getline(in, line)) {
            // Ensures empty lines are not logged
            if (!should_log_line(line)) {
                continue;
            }
            logger->logFile << prefix << " " << line << std::endl;
        }
        logger->logFile.flush();
    } else {
        // Build string first to minimize risk of stdout buffer interleaving during mpi runs
        std::ostringstream oss;
        while (std::getline(in, line)) {
            // Ensures empty lines are not logged
            if (!should_log_line(line)) {
                continue;
            }
            oss.str("");     // clear the contents
            oss.clear();     // reset stream state flags
            oss << prefix << " " << line << '\n';
            std::cout << oss.str() << std::flush;
        }
    }
}

void Logger::Log(const std::string& message, LogLevel messageLevel) {
    Logger* logger = GetLogger();
    Log(logger->ewtsId, messageLevel, message);
}

std::string Logger::LevelToFixedString(LogLevel level) {
    const std::string name = std::string(ewts::LogLevelName(static_cast<int>(level)));
    // pad/truncate to 7 chars
    std::string out = name;
    if (out.size() < 7) out.append(7 - out.size(), ' ');
    if (out.size() > 7) out = out.substr(0, 7);
    return out;
}

std::string Logger::CreateTimestamp(bool append_ms, bool iso) {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto secs = time_point_cast<seconds>(now);
    const auto ms = duration_cast<milliseconds>(now - secs).count();

    std::time_t t = system_clock::to_time_t(now);
    std::tm tm_utc;

    gmtime_r(&t, &tm_utc);

    std::ostringstream oss;
    if (iso) {
        oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%S");
        if (append_ms) {
            oss << "." << std::setw(3) << std::setfill('0') << ms;
        }
        oss << "Z";
    } else {
        oss << std::put_time(&tm_utc, "%Y-%m-%d %H:%M:%S");
        if (append_ms) {
            oss << "." << std::setw(3) << std::setfill('0') << ms;
        }
    }
    return oss.str();
}

std::string Logger::CreateCompactTimestampUTC() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm_utc;

    gmtime_r(&t, &tm_utc);

    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y%m%dT%H%M%S");
    return oss.str();
}

bool Logger::FileExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool Logger::DirectoryExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool Logger::CreateDirectory(const std::string& path) {
    if (path.empty()) return false;
    if (DirectoryExists(path)) return true;

    const std::string cmd = "mkdir -p \"" + path + "\"";
    const int status = std::system(cmd.c_str());
    if (status == -1) return false;

    // status is shell-dependent; treat nonzero as failure
    return DirectoryExists(path);
}

std::string Logger::GetHomeDir() {
    const char* h = std::getenv("HOME");
    if (h && std::strlen(h) > 0) return std::string(h);
    return ".";
}

std::string Logger::JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

std::string Logger::EnvVarIdentFromModuleKey(const std::string& key) {
    if (key.empty()) return "";

    // First try exact match
    if (const char* id = ewts_ngen::EwtsIdFromKey(key.c_str())) {
        return std::string(id);
    }

    // Then try lowercased key (registry keys are typically lowercase)
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    if (const char* id = ewts_ngen::EwtsIdFromKey(lower.c_str())) {
        return std::string(id);
    }
    return "";
}

bool Logger::PayloadFileReady(void) const {
    return payloadFile.is_open() && payloadFile.good();
}

bool Logger::OpenPayloadFileIfNeeded(void) {
    if (PayloadFileReady()) {
        return true;
    }

    InitIfNeeded();

    if (!loggingEnabled) {
        return false;
    }

    if (logFileDir.empty()) {
        return false;
    }

    std::string stem = "ngen_payload";
    stem = PrependLogFilePrefix(stem);

    std::string rank_part;
    if (mpi_is_initialized()) {
        rank_part = "_" + kLogRankLabel + "_" + std::to_string(GetRank());
    }

    std::string ts_part;
    const char* rd = std::getenv(kEnvResultsDir);
    if (!(rd && std::strlen(rd) > 0)) {
        ts_part = "_" + CreateCompactTimestampUTC();
    }

    const std::string filename = stem + rank_part + ts_part + ".log";
    payloadFilePath = JoinPath(logFileDir, filename);

    payloadFile.open(payloadFilePath.c_str(), std::ios::out | std::ios::trunc);

    if (PayloadFileReady()) {
        std::ostringstream oss;
        if (g_mpiRank >= 0) {
            oss << "[rank " << g_mpiRank << "] ";
        }
        oss << "EWTS NGEN payload log file " << payloadFilePath << '\n';
        std::cout << oss.str() << std::flush;
        return true;
    }

    return false;
}

bool Logger::LogPayload(const char* ewts_id, const char* json_message)
{
    if (!json_message) {
        LogPayload(
            ewts_id,
            ewts::PAYLOAD_ERROR,
            0.0,
            "Malformed payload: payload message is null",
            "");
        return false;
    }

    const PayloadExtractResult extracted = ExtractJsonPayload(json_message);

    if (!extracted.ok) {
        LogPayload(
            ewts_id,
            ewts::PAYLOAD_ERROR,
            -1.0,
            extracted.error_msg,
            "");
        return false;
    }

    try
    {
        std::stringstream ss(extracted.json);

        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        LogPayload(
            ewts_id,
            pt.get<std::string>("status", ""),
            pt.get<double>("prog", -1.0),
            pt.get<std::string>("msg", ""),
            pt.get<std::string>("modnm", "")
        );
        return true;
    }
    catch (const boost::property_tree::json_parser_error& e)
    {
        LogPayload(
            ewts_id,
            ewts::PAYLOAD_ERROR,
            -1.0,
            std::string("Malformed payload JSON: ") + e.what(),
            "");
        return false;
    }
    catch (const boost::property_tree::ptree_error& e)
    {
        LogPayload(
            ewts_id,
            ewts::PAYLOAD_ERROR,
            -1.0,
            std::string("Malformed payload fields: ") + e.what(),
            "");
        return false;
    }
}

void Logger::LogPayload(
    const char* ewts_id,
    const std::string& status,
    double prog,
    const std::string& msg,
    const std::string& modnm)
{
    Logger* logger = GetLogger();

    if (!logger->OpenPayloadFileIfNeeded()) {
        return;
    }

    std::ostringstream json;
    bool first = true;

    auto add_string = [&](const char* key, const std::string& value)
    {
        if (!first) json << ',';
        first = false;
        json << '"' << key << "\":";
        if (value.empty() || value == ewts::PAYLOAD_NULL) {
            json << "null";
        } else {
            json << '"' << EscapeJson(value) << '"';
        }
    };

    auto add_double = [&](const char* key, double value)
    {
        if (!first) json << ',';
        first = false;
        json << '"' << key << "\":";
        if (value < 0.0) {
            json << "null";
        } else {
            json << std::defaultfloat << value;
        }
    };

    json << '{';

    add_string("status", status);
    add_double("prog", prog);
    add_string("msg", msg);
    add_string("modnm", modnm);

    json << '}';

    const std::string module_name =
    (ewts_id && std::strlen(ewts_id) > 0)
        ? std::string(ewts_id)
        : logger->ewtsId;

    const std::string prefix =
        CreateTimestamp(true, true) + " " +
        PadEwtsId(module_name) + " " +
        LevelToFixedString(LogLevel::STATUS) + " ";

    const std::string record =
        prefix +
        std::string(kPayloadBeginSentinel) +
        json.str() +
        std::string(kPayloadEndSentinel) +
        "\n";

    static std::mutex payloadLogMutex;
    {
        std::lock_guard<std::mutex> lock(payloadLogMutex);

        logger->payloadFile << record;
        logger->payloadFile.flush();
    }
}
