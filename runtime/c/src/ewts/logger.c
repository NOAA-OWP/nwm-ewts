#include "ewts/logger.h"

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Optional NGEN bridge. Present only when linked into an NGEN build. */
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

#define EV_NGEN_RESULTS_DIR "NGEN_RESULTS_DIR"
#define EV_EWTS_ENABLED     "EWTS_ENABLED"
#define EV_EWTS_LOG_DIR     "EWTS_LOG_DIR"
#define EV_EWTS_LOG_LEVEL   "EWTS_LOG_LEVEL"

static int g_mpiRank = -1;

typedef struct ewts_logger_state {
    char ewts_id[64];
    char ewts_id_padded[9];
    int use_ngen;
    int initialized;
    int enabled;
    LogLevel level;
    FILE* file;
    char path[1024];
    struct ewts_logger_state* next;
} ewts_logger_state;

static pthread_mutex_t g_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_write_mutex    = PTHREAD_MUTEX_INITIALIZER;
static ewts_logger_state* g_loggers     = NULL;

static int streq_ci(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static void trim_copy(const char* in, char* out, size_t out_sz) {
    size_t len = 0;
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!in) return;

    while (isspace((unsigned char)*in)) ++in;
    len = strlen(in);
    while (len > 0 && isspace((unsigned char)in[len - 1])) --len;
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, in, len);
    out[len] = '\0';
}

static void upper_copy(const char* in, char* out, size_t out_sz) {
    size_t i = 0;
    char tmp[128];
    trim_copy(in, tmp, sizeof(tmp));
    for (; tmp[i] != '\0' && i + 1 < out_sz; ++i) {
        out[i] = (char)toupper((unsigned char)tmp[i]);
    }
    out[i] = '\0';
}

static int is_ngen_active(void) {
    const char* v = getenv(EV_NGEN_RESULTS_DIR);
    return (v && v[0] != '\0');
}

static int parse_enabled(const char* v) {
    if (!v || v[0] == '\0') return 1;
    char s[32];
    trim_copy(v, s, sizeof(s));
    if (s[0] == '\0') return 1;

    if (streq_ci(s, "0") || streq_ci(s, "false") || streq_ci(s, "no") ||
        streq_ci(s, "off") || streq_ci(s, "disabled")) {
        return 0;
    }
    return 1;
}

static LogLevel parse_level(const char* v) {
    if (!v || v[0] == '\0') return NOTSET;
    char s[32];
    trim_copy(v, s, sizeof(s));
    if (s[0] == '\0') return NOTSET;

    {
        char* end = NULL;
        long num = strtol(s, &end, 10);
        if (end && *end == '\0' && num >= 0) return (LogLevel)num;
    }

    if (streq_ci(s, "DEBUG")) return DEBUG;
    if (streq_ci(s, "PERFORM")) return PERFORM;
    if (streq_ci(s, "INFO")) return INFO;
    if (streq_ci(s, "WARN") || streq_ci(s, "WARNING")) return WARNING;
    if (streq_ci(s, "ERROR") || streq_ci(s, "SEVERE")) return SEVERE;
    if (streq_ci(s, "FATAL") || streq_ci(s, "CRITICAL")) return FATAL;
    if (streq_ci(s, "STATUS") || streq_ci(s, "STATUS")) return STATUS;
    if (streq_ci(s, "NOTSET") || streq_ci(s, "NONE")) return NOTSET;

    return NOTSET;
}

static const char* level_name_padded(LogLevel lvl) {
    switch ((int)lvl) {
        case DEBUG:   return "DEBUG  ";
        case PERFORM: return "PERFORM";
        case INFO:    return "INFO   ";
        case WARNING: return "WARNING";
        case SEVERE:  return "SEVERE ";
        case FATAL:   return "FATAL  ";
        case STATUS:  return "STATUS ";
        default:      return "NOTSET ";
    }
}

static void utc_timestamp_iso_ms(char* buf, size_t sz) {
    struct timeval tv;
    struct tm tm_utc;
    size_t n;
    unsigned int ms;
    if (!buf || sz == 0) return;

    gettimeofday(&tv, NULL);
    gmtime_r(&tv.tv_sec, &tm_utc);

    if (sz < 25) {
        buf[0] = '\0';
        return;
    }

    n = strftime(buf, sz, "%Y-%m-%dT%H:%M:%S", &tm_utc);
    if (n == 0) {
        buf[0] = '\0';
        return;
    }

    ms = (unsigned int)(tv.tv_usec / 1000);
    if (ms > 999) ms = 999;

    buf[n++] = '.';
    buf[n++] = (char)('0' + (ms / 100) % 10);
    buf[n++] = (char)('0' + (ms / 10) % 10);
    buf[n++] = (char)('0' + (ms % 10));
    buf[n++] = 'Z';
    buf[n] = '\0';
}

static void utc_timestamp_compact(char* buf, size_t sz) {
    time_t t = time(NULL);
    struct tm tm_utc;
    gmtime_r(&t, &tm_utc);
    strftime(buf, sz, "%Y%m%dT%H%M%S", &tm_utc);
}

static int dir_exists(const char* path) {
    struct stat st;
    return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
}

static int mkdir_p(const char* path) {
    char tmp[1024];
    size_t len;
    char* p;

    if (!path || path[0] == '\0') return 0;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return 0;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (!dir_exists(tmp)) {
                if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return 0;
            }
            *p = '/';
        }
    }
    if (!dir_exists(tmp)) {
        if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return 0;
    }
    return 1;
}

static void build_module_loglevel_env(const char* ewts_id, char* out, size_t out_sz) {
    size_t n = 0;
    const char* suffix = "_LOGLEVEL";
    if (!out || out_sz == 0) return;
    out[0] = '\0';

    for (; ewts_id && *ewts_id && n + 1 < out_sz; ++ewts_id) {
        out[n++] = (char)toupper((unsigned char)*ewts_id);
    }
    for (; *suffix && n + 1 < out_sz; ++suffix) {
        out[n++] = *suffix;
    }
    out[n] = '\0';
}

static void pad_ewts_id(ewts_logger_state* st) {
    size_t i = 0;
    for (; i < 8 && st->ewts_id[i] != '\0'; ++i) {
        st->ewts_id_padded[i] = (char)toupper((unsigned char)st->ewts_id[i]);
    }
    for (; i < 8; ++i) {
        st->ewts_id_padded[i] = ' ';
    }
    st->ewts_id_padded[8] = '\0';
}

static void open_standalone_file(ewts_logger_state* st) {
    char log_dir[1024];
    char ts[32];
    const char* dir;
    int n;

    if (st->file) return;

    dir = getenv(EV_EWTS_LOG_DIR);
    if (!(dir && dir[0] != '\0')) {
        st->file = stdout;
        return;
    }
    snprintf(log_dir, sizeof(log_dir), "%s", dir);
    (void)mkdir_p(log_dir);
    utc_timestamp_compact(ts, sizeof(ts));

    n = snprintf(st->path, sizeof(st->path), "%s/%s_%s.log", log_dir, st->ewts_id, ts);
    if (n < 0 || (size_t)n >= sizeof(st->path)) {
        n = snprintf(st->path, sizeof(st->path), "%s/%s.log", log_dir, st->ewts_id);
        if (n < 0 || (size_t)n >= sizeof(st->path)) {
            fprintf(stderr,
                    "EWTS ERROR: Log path too long (dir='%s', id='%s'). Falling back to stdout.\n",
                    log_dir, st->ewts_id);
            st->path[0] = '\0';
            st->file = stdout;
            return;
        } else {
            fprintf(stderr,
                    "EWTS WARNING: Log path truncated using shorter filename '%s'.\n",
                    st->path);
        }
    }

    st->file = fopen(st->path, "a");
    if (!st->file) {
        fprintf(stderr,
                "EWTS ERROR: Failed to open log file '%s'. Falling back to stdout.\n",
                st->path);
        st->file = stdout;
    }
}

static void init_logger_state(ewts_logger_state* st) {
    char key[64];
    LogLevel lvl;

    if (st->initialized) return;
    st->initialized = 1;

    const char* val = getenv("EWTS_RANK");
    g_mpiRank = val ? atoi(val) : -1;
    
    char prefix[64];

    if (g_mpiRank >= 0) {
        snprintf(prefix, sizeof(prefix), "[rank %d] EWTS", g_mpiRank);
    } else {
        snprintf(prefix, sizeof(prefix), "EWTS");
    }

    pad_ewts_id(st);

    st->enabled = parse_enabled(getenv(EV_EWTS_ENABLED));
    fprintf(stdout, "%s %s logging is %s\n", prefix, st->ewts_id, st->enabled ? "ENABLED" : "DISABLED");
    fflush(stdout);

    build_module_loglevel_env(st->ewts_id, key, sizeof(key));
    lvl = parse_level(getenv(key));
    fprintf(stdout, "%s %s log level from env var %s is %s\n",
            prefix, st->ewts_id, key, level_name_padded(lvl));
    fflush(stdout);

    if ((int)lvl != NOTSET) {
        st->level = lvl;
        fprintf(stdout, "%s %s log level set to %s\n",
                prefix, st->ewts_id, level_name_padded(st->level));
        fflush(stdout);
    } else {
        lvl = parse_level(getenv(EV_EWTS_LOG_LEVEL));
        st->level = ((int)lvl != NOTSET) ? lvl : INFO;
        fprintf(stdout, "%s %s using default log level = %s\n",
                prefix, st->ewts_id, level_name_padded(st->level));
        fflush(stdout);
    }

    if (st->use_ngen && is_ngen_active() && ewts_ngen_log) {
        fprintf(stdout, "%s %s using ngen for logging\n", prefix, st->ewts_id);
    } else {
        fprintf(stdout, "%s %s logging standalone\n", prefix, st->ewts_id);
    }
    fflush(stdout);
}

static ewts_logger_state* get_logger_state(const char* ewts_id, int ewts_ngen) {
    ewts_logger_state* cur = NULL;
    char key[64];
    upper_copy((ewts_id && ewts_id[0] != '\0') ? ewts_id : "EWTS", key, sizeof(key));

    pthread_mutex_lock(&g_registry_mutex);

    cur = g_loggers;
    while (cur) {
        if (strcmp(cur->ewts_id, key) == 0) {
            if (ewts_ngen) cur->use_ngen = 1;
            if (!cur->initialized) init_logger_state(cur);
            pthread_mutex_unlock(&g_registry_mutex);
            return cur;
        }
        cur = cur->next;
    }

    cur = (ewts_logger_state*)calloc(1, sizeof(*cur));
    if (!cur) {
        pthread_mutex_unlock(&g_registry_mutex);
        return NULL;
    }

    snprintf(cur->ewts_id, sizeof(cur->ewts_id), "%s", key);
    cur->use_ngen = ewts_ngen ? 1 : 0;
    cur->enabled = 1;
    cur->level = INFO;
    cur->initialized = 0;
    cur->file = NULL;
    cur->path[0] = '\0';

    cur->next = g_loggers;
    g_loggers = cur;

    init_logger_state(cur);
    pthread_mutex_unlock(&g_registry_mutex);
    return cur;
}

static void v_log_with_state(ewts_logger_state* st, LogLevel level, const char* fmt, va_list ap) {
    va_list ap2;
    int n;
    char* msg;
    char ts[32];
    const char* lvl_str;

    if (!st || !fmt) return;
    if (!st->enabled) return;
    if ((int)level < (int)st->level) return;

    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) return;

    msg = (char*)malloc((size_t)n + 1);
    if (!msg) return;
    vsnprintf(msg, (size_t)n + 1, fmt, ap);

    if (st->use_ngen && is_ngen_active() && ewts_ngen_log) {
        ewts_ngen_log(st->ewts_id, (int)level, msg);
        free(msg);
        return;
    }

    utc_timestamp_iso_ms(ts, sizeof(ts));
    lvl_str = level_name_padded(level);

    pthread_mutex_lock(&g_write_mutex);
    open_standalone_file(st);

    {
        FILE* out = st->file ? st->file : stdout;
        char* saveptr = NULL;
        char* line = strtok_r(msg, "\n", &saveptr);

        if (!line) {
            fprintf(out, "%s %s %s\n", ts, st->ewts_id_padded, lvl_str);
        } else {
            while (line) {
                fprintf(out, "%s %s %s %s\n", ts, st->ewts_id_padded, lvl_str, line);
                line = strtok_r(NULL, "\n", &saveptr);
            }
        }

        fflush(out);
    }

    pthread_mutex_unlock(&g_write_mutex);
    free(msg);
}

void EwtsInit(const char* ewts_id, bool ewts_ngen) {
    (void)get_logger_state(ewts_id, ewts_ngen ? 1 : 0);
}

void EwtsLogModule(const char* ewts_id, LogLevel level, const char* fmt, ...) {
    ewts_logger_state* st;
    va_list ap;

    st = get_logger_state(ewts_id, 0);
    if (!st || !fmt) return;

    va_start(ap, fmt);
    v_log_with_state(st, level, fmt, ap);
    va_end(ap);
}

LogLevel EwtsGetLogLevelModule(const char* ewts_id) {
    ewts_logger_state* st = get_logger_state(ewts_id, 0);
    return st ? st->level : INFO;
}

bool EwtsIsLoggingEnabledModule(const char* ewts_id) {
    ewts_logger_state* st = get_logger_state(ewts_id, 0);
    return st ? (st->enabled ? true : false) : true;
}

/*
 * Compatibility wrappers.
 *
 * These preserve the original API shape. If callers do not define EWTS_ID and
 * still use bare Log(...), the fallback logger ID is EWTS.
 */
void Log(LogLevel level, const char* fmt, ...) {
    ewts_logger_state* st;
    va_list ap;

    st = get_logger_state("EWTS", 0);
    if (!st || !fmt) return;

    va_start(ap, fmt);
    v_log_with_state(st, level, fmt, ap);
    va_end(ap);
}

LogLevel GetLogLevel(void) {
    return EwtsGetLogLevelModule("EWTS");
}

bool IsLoggingEnabled(void) {
    return EwtsIsLoggingEnabledModule("EWTS");
}

void EwtsPayloadStatus(
    const char* ewts_id,
    const char* status,
    double prog,
    const char* msg,
    const char* modnm)
{
    if (is_ngen_active() && ewts_ngen_payload_status) {
        ewts_ngen_payload_status(ewts_id, status, prog, msg, modnm);
    }
}
