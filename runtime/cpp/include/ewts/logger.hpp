
#ifndef EWTS_LOGGER_HPP
#define EWTS_LOGGER_HPP

#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include "ewts/payload_status.hpp"

namespace ewts {

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
    Logger(std::string ewts_id, bool ewts_ngen);

    bool IsLoggingEnabled();
    LogLevel GetLogLevel();

    void Log(LogLevel level, std::string_view message);
    void Log(LogLevel level, const char* fmt, ...);
    void Log(std::string_view message, LogLevel level);

private:
    void init_once();
    void open_standalone_file();
    bool have_ngen_bridge() const;
    std::string module_loglevel_env() const;
    void pad_id();

private:
    std::string ewts_id_;
    std::string ewts_id_padded_;
    bool use_ngen_ = false;
    bool initialized_ = false;
    bool enabled_ = true;
    LogLevel level_ = LogLevel::INFO;

    std::string path_;

    mutable std::mutex init_mtx_;
    mutable std::mutex log_mtx_;
    std::ofstream out_;
};

Logger& GetLogger(std::string_view ewts_id, bool ewts_ngen = true);
Logger& CurrentLogger();

/* Compatibility API */
void EwtsInit(std::string_view ewts_id, bool ewts_ngen = true);
bool IsLoggingEnabled();
LogLevel GetLogLevel();
void Log(LogLevel level, std::string_view message);
void Log(LogLevel level, const char* fmt, ...);
void Log(std::string_view message, LogLevel level);


void PayloadStatus(
    const char* ewts_id,
    const char* status=ewts::PAYLOAD_NULL,
    double prog=-1.0,
    const char* msg="",
    const char* modnm="");

} // namespace ewts

#define PAYLOAD_STATUS(ewts_id, status, prog, msg, modnm) \
    ::ewts::PayloadStatus((ewts_id), (status), (prog), (msg), (modnm))

#endif
