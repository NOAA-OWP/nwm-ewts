#ifndef EWTS_LOGGER_H
#define EWTS_LOGGER_H

#include <stdbool.h>
#include "ewts/payload_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NOTSET  = 0,
    DEBUG   = 10,
    PERFORM = 15,
    INFO    = 20,
    WARNING = 30,
    SEVERE  = 40,
    FATAL   = 50,
    STATUS  = 60
} LogLevel;

/* Compatibility API */
void EwtsInit(const char* ewts_id, bool ewts_ngen);
void Log(LogLevel level, const char* fmt, ...);
LogLevel GetLogLevel(void);
bool IsLoggingEnabled(void);

/* Explicit per-module API */
void EwtsLogModule(const char* ewts_id, LogLevel level, const char* fmt, ...);
LogLevel EwtsGetLogLevelModule(const char* ewts_id);
bool EwtsIsLoggingEnabledModule(const char* ewts_id);


void EwtsPayloadStatus(
    const char* ewts_id,
    const char* status,
    double prog,
    const char* msg,
    const char* modnm);

#define PAYLOAD_STATUS(ewts_id, status, prog, msg, modnm) \
    EwtsPayloadStatus((ewts_id), (status), (prog), (msg), (modnm))
    
#ifdef __cplusplus
}
#endif
    
#endif /* EWTS_LOGGER_H */
