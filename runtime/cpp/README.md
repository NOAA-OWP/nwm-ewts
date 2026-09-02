# EWTS C++ Runtime

The Error and Warning Trapping System (EWTS) CPP runtime provides logging support for 
CPP modules. When linked\with the optional ngen bridge, log messages are forwarded into 
the ngenlogging infrastructure. Otherwise, log messages are written directly by
the CPP runtime, using the directory specified by the `EWTS_LOG_DIR`
environment variable when available, or stdout if no usable log
directory is configured.

It supports two logging modes:

- **ngen-integrated logging**, where messages are forwarded through the optional ngen bridge.
- **standalone logging**, where messages are written directly by the CPP runtime without the ngen bridge.

The runtime shares the same logging model, log levels, payload format, and
environment-driven configuration as the C, Fortran, and Python EWTS runtime
libraries.

## Directory role

The implementation in `runtime/cpp/` provides:

- Native C++ logger implementation.
- Generated C++ module IDs, log-level constants, and payload status constants.
- Module-scoped logger identity for shared runtime execution.
- Optional bridging support for `ngen`-integrated execution.

## Design goals

The C++ runtime is designed to provide:

- Consistent behavior with the C, Fortran, and Python runtime libraries.
- Module-scoped logger identity when multiple modules execute in the same process.
- Environment-driven configuration.
- Transparent forwarding of log messages to the ngen logging infrastructure when available.
- A standalone fallback that works without ngen or the ngen bridge library.

## Shared-runtime behavior

Multiple modules may execute within the same process. Each logger is therefore
associated with a specific EWTS module ID rather than being process-global,
ensuring that log messages are correctly attributed regardless of the execution
environment.

Installed headers are exposed under `include/ewts/`.

## Public API

```cpp
namespace ewts {

class Logger {
public:
    Logger(std::string ewts_id = "EWTS", bool ewts_ngen = false);

    bool IsLoggingEnabled();
    LogLevel GetLogLevel();

    void Log(LogLevel level, std::string_view message);
    void Log(std::string_view message, LogLevel level = LogLevel::INFO);
    void Log(LogLevel level, const char* fmt, ...);
};

Logger& GetLogger(std::string_view ewts_id = "EWTS", bool ewts_ngen = false);
Logger& CurrentLogger();

void EwtsInit(std::string_view ewts_id, bool ewts_ngen = false);

bool IsLoggingEnabled();
LogLevel GetLogLevel();

void Log(LogLevel level, std::string_view message);
void Log(std::string_view message, LogLevel level = LogLevel::INFO);

void PayloadStatus(
    const char* ewts_id,
    const char* status,
    double prog,
    const char* msg,
    const char* modnm);

} // namespace ewts

#define PAYLOAD_STATUS(ewts_id, status, prog, msg, modnm) \
    ::ewts::PayloadStatus((ewts_id), (status), (prog), (msg), (modnm))

```

## Log levels

| Level | Value |
|-------|------:|
| `NOTSET` | 0 |
| `DEBUG` | 10 |
| `PERFORM` | 15 |
| `INFO` | 20 |
| `WARNING` | 30 |
| `SEVERE` | 40 |
| `FATAL` | 50 |
| `STATUS` | 60 |

`STATUS` is reserved for structured payload messages when running under `ngen`.

## Payload status constants

The C++ runtime provides predefined constants for the standard EWTS payload
status values. These constants should be used when calling `PAYLOAD_STATUS` to
ensure consistent status reporting across all runtime libraries.

| Constant | String Value | Description |
|----------|--------------|-------------|
| `PAYLOAD_NULL` | `"NULL"` | No status has been assigned. |
| `PAYLOAD_INITTING` | `"INITIALIZING"` | Module initialization is in progress. |
| `PAYLOAD_INITTED` | `"INITIALIZED"` | Module initialization has completed successfully. |
| `PAYLOAD_STARTING` | `"STARTING"` | Module execution is beginning. |
| `PAYLOAD_INPROG` | `"IN_PROGRESS"` | Module execution is currently in progress. |
| `PAYLOAD_COMPLETE` | `"COMPLETE"` | Module execution completed successfully. |
| `PAYLOAD_ERROR` | `"ERROR"` | Module execution terminated with an error. |

## Standard logging

Consumer Logger.hpp
```cpp
#include "ewts/module_constants.hpp"
#include "ewts/logger.hpp"
#include "ewts/log_levels.hpp"

#define LOG(...) ::ewts::GetLogger(::ewts::modules::EWTS_ID_SFT).Log(__VA_ARGS__)
#define GetLogLevel() ::ewts::GetLogger(::ewts::modules::EWTS_ID_SFT).GetLogLevel()
#define IsLoggingEnabled() ::ewts::GetLogger(::ewts::modules::EWTS_ID_SFT).IsLoggingEnabled()

using ewts::EwtsInit;
using ewts::LogLevel;

inline constexpr const char* SFT_MODULE_ID = ewts::modules::EWTS_ID_SFT;
```

Consumer module

``` cpp
#include "Logger.hpp"

  LOG(LogLevel::INFO, "Initializing SFT");
```


## Payload logging

C++ modules can write structured STATUS payloads with `PAYLOAD_STATUS(...)`:

``` c
PAYLOAD_STATUS(
    EWTS_ID_SFT,
    "INITIALIZING",
    0.1,
    "SFT Initializing",
    "SFT");
```

Arguments:

| Argument | Meaning |
|----------|---------|
| `ewts_id` | EWTS ID used in the payload log prefix |
| `status` | Payload status value, such as `INITIALIZING` or `IN_PROGRESS` |
| `prog` | Progress value, usually `0.0` through `1.0` but can be any value|
| `msg` | Human-readable payload message |
| `modnm` | Module/component name written into the JSON payload |

Payload logging is active only when `EWTS_USE_NGEN_BRIDGE` is enabled
and the optional `ewts_ngen_payload_status` bridge function is
available. Otherwise, `EwtsPayloadStatus()` performs no action.


## Runtime relationship to `ngen`

When `EWTS_USE_NGEN_BRIDGE` is enabled and the optional `ewts_ngen_bridge`
library is linked into the application, the runtime forwards log messages into
the ngen logging infrastructure. The integration layer owns log routing, output
location, and file management while the runtime remains responsible for log
formatting and module attribution.

## Environment configuration

| Variable | Purpose |
|----------|---------|
| `EWTS_USE_NGEN_BRIDGE` | Enables logging through the ngen bridge when set to a truthy value (`1`, `true`, `yes`, `on`, etc.). |
| `EWTS_ENABLED` | Enables or disables logging. |
| `EWTS_LOG_LEVEL` | Default log level; `INFO` if undefined. |
| `EWTS_RANK` | Optional MPI rank used in initialization messages. |
| `<MODULE>_LOGLEVEL` | Per-module log level override. |

## Runtime behavior

### ngen bridge

When `EWTS_USE_NGEN_BRIDGE` is enabled and the optional
`ewts_ngen_bridge` library is available, log messages are forwarded to the
ngen logging infrastructure.

Structured STATUS payloads generated through `PayloadStatus()` are also
forwarded through the bridge.

### Stdout fallback

If the ngen bridge is not enabled or is unavailable, log messages are written
to standard output. This provides a simple fallback for running modules outside
of ngen while preserving the same logging API.

## MPI behavior

When running under MPI, the runtime prefixes initialization messages with the
MPI rank when `EWTS_RANK` is defined. Runtime log routing is handled by the
selected logging backend (ngen bridge or stdout fallback).

## Module CMakeLists Update

```cmake
option(USE_EWTS "Build SFT with EWTS logging" ON)
message("-- SFT CMakeLists USE_EWTS = ${USE_EWTS}")
if(USE_EWTS)
    message("-- SFT compiled with EWTS logging")

    # --- EWTS (installed from nwm-ewts) ---
    find_package(ewts CONFIG REQUIRED)

    # Always use EWTS runtime logger for CPP
    target_link_libraries(sftbmi PRIVATE ewts::ewts_cpp)

    # Built with ngen bridge
    target_link_libraries(sftbmi PRIVATE 
        "-Wl,--no-as-needed" ewts::ewts_ngen_bridge "-Wl,--as-needed")
    target_compile_definitions(sftbmi PRIVATE SFT_USE_EWTS)
else()
    message("-- SFT compiled with stdout fallback logging")
endif()
```

## Related documentation

- User-facing overview: `docs/runtimes/cpp.md`
- `ngen` integration: `integrations/ngen/README.md`
- Generated constants workflow: `tools/README.md`
