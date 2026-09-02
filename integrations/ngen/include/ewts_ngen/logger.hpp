#ifndef EWTS_NGEN_LOGGER_HPP
#define EWTS_NGEN_LOGGER_HPP

#include <cstdarg>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

#define BOOST_BIND_GLOBAL_PLACEHOLDERS // intentionally want the old behavior
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "ewts_ngen/ngen_module_constants.hpp"
#include "ewts/payload_status.hpp"

/*
 * NGEN integration logger.
 *
 * - Owns all logging policy (config/env/file/format) in logger.cpp
 * - Reads <NGEN_RESULTS_DIR>/ngen_logging.json when NGEN_RESULTS_DIR is set
 * - Exports EWTS_ENABLED and <MODULE>_LOGLEVEL environment variables
 * - Supports optional MPI rank suffix
 * - Falls back to EWTS_LOG_DIR for standalone logging when NGEN_RESULTS_DIR is unavailable
 */

enum class LogLevel : int {
    NOTSET  = 0,
    DEBUG   = 10,
    PERFORM = 15,
    INFO    = 20,
    WARNING = 30,
    SEVERE  = 40,
    FATAL   = 50,
    STATUS  = 60
};

class Logger {
  public:
    static Logger* GetLogger();
    static int g_mpiRank;

    // Backwards-compatible overloads
    static void Log(const std::string& message, LogLevel messageLevel = LogLevel::INFO);
    static void Log(LogLevel messageLevel, const std::string& message);
    static void Log(LogLevel messageLevel, const char* message, ...);

    // ewtsID-aware overloads
    static void Log(const std::string& moduleName, LogLevel messageLevel, const std::string& message);
    static void Log(const std::string& moduleName, LogLevel messageLevel, const char* message, ...);

    // Look into deleting this in the future. This was kept for backward compatability but
    // is a poor design. The throw is misleading from here.
    static inline void LogAndThrow(const std::string& message) {
        Log(message, LogLevel::SEVERE);
        throw std::runtime_error(message);
    }

    bool IsLoggingEnabled() const { return loggingEnabled; }
    LogLevel GetLogLevel() const { return logLevel; }
    
    static void LogPayload(
        const char* ewts_id,
        const std::string& status=std::string(),
        double             prog=-1.0,
        const std::string& msg=std::string(),
        const std::string& modnm=std::string());

    static bool LogPayload(
        const char* ewts_id,
        const char* json_message);

  private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // init/policy
    void InitIfNeeded();
    bool ReadConfigFromResultsDir(const std::string& resultsDir);
    void ConfigureEnvVars(bool set);

    // log file
    void SetupLogFile(const std::string& resultsDir);
    bool LogFileReady() const;

    // helpers
    static std::string CreateTimestamp(bool append_ms = true, bool iso = true);
    static std::string CreateCompactTimestampUTC();  // YYYYMMDDTHHMMSS
    static std::string LevelToFixedString(LogLevel level);

    static bool FileExists(const std::string& path);
    static bool DirectoryExists(const std::string& path);
    static bool CreateDirectory(const std::string& path);
    static bool FindConfigFileFromPath(std::string path, std::string& configPath);
    static std::string GetParentDirName(const std::string& path);

    static std::string GetHomeDir();
    static std::string JoinPath(const std::string& a, const std::string& b);
    static std::string EnvVarIdentFromModuleKey(const std::string& key);
    static std::string GetStandaloneBaseDir();

    int GetRank() const { return g_mpiRank; }

    // state
    bool        loggingEnabled  = true;
    bool        splitLogsByModule = false;

    // Normal EWTS/ngen log stream.
    std::fstream logFile;
    std::string  logFileDir;
    std::string  logFilePath;

    // module identity (stable key is lower case; ewts_id is upper case for log messages)
    std::string  moduleKey = "ngen";
    std::string  ewtsId    = "NGEN";

    LogLevel     logLevel = LogLevel::INFO;

    // config-derived per-module levels (stable key -> LogLevel)
    std::unordered_map<std::string, LogLevel> moduleLogLevels;

    // Lazily opened payload stream.
    // This is never split by module and is created only on first payload write.
    std::ofstream payloadFile;
    std::string payloadFilePath;

    bool PayloadFileReady(void) const;
    bool OpenPayloadFileIfNeeded(void);

    // environment
    
};

// Placed here to ensure the class is declared before setting these preprocessor symbols
#define LOG Logger::Log
#define GetLogLevel() Logger::GetLogger()->GetLogLevel()
#define IsLoggingEnabled() Logger::GetLogger()->IsLoggingEnabled()

#define PAYLOAD_STATUS(ewts_id, status, prog, msg, modnm) \
    Logger::LogPayload((ewts_id), (status), (prog), (msg), (modnm))

#endif /* EWTS_NGEN_LOGGER_HPP */
