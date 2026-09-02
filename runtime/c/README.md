# EWTS C Runtime

The Error and Warning Trapping System (EWTS) C runtime provides logging 
support for C modules. When linked with the optional ngen bridge, log 
messages are forwarded into the ngen logging infrastructure. Otherwise, 
log messages are written directly by the C runtime, using the directory
specified by the `EWTS_LOG_DIR` environment variable when available, or
stdout if no usable log directory is configured.

It supports two logging modes:

- **ngen-integrated logging**, where messages are forwarded through the optional ngen bridge.
- **standalone logging**, where messages are written directly by the C runtime without the ngen bridge.

This runtime shares the same logging model, log levels, and environment-driven
configuration as the CPP, Fortran, and Python EWTS runtime libraries.

## Directory role

The implementation in `runtime/c/` provides:

- Native C logger implementation.
- Generated C module IDs, log-level constants, and payload status constants.
- Module-scoped logger identity for shared runtime execution.
- Optional bridging support for `ngen`-integrated execution.

## Design goals

The C runtime is designed to provide:

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

``` c
void EwtsInit(const char* ewts_id, bool ewts_ngen);
void Log(LogLevel level, const char* fmt, ...);
LogLevel GetLogLevel(void);
bool IsLoggingEnabled(void);

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

`STATUS` is reserved for structured payload messages when running under
`ngen`.

## Payload status constants

The C runtime provides generated payload status string constants in `ewts/payload_status.h`.

| Constant | Value |
|---|---|
| `PAYLOAD_NULL` | `"NULL"` |
| `PAYLOAD_INITTING` | `"INITIALIZING"` |
| `PAYLOAD_INITTED` | `"INITIALIZED"` |
| `PAYLOAD_STARTING` | `"STARTING"` |
| `PAYLOAD_INPROG` | `"IN_PROGRESS"` |
| `PAYLOAD_COMPLETE` | `"COMPLETE"` |
| `PAYLOAD_ERROR` | `"ERROR"` |

## Standard logging

Consumer logger.h
```c
#include "ewts/module_constants.h"
#include "ewts/logger.h"
#include "ewts/log_levels.h"

#define CFE_MODULE_ID EWTS_ID_CFE

#define Log(level, ...) EwtsLogModule(CFE_MODULE_ID, (level), __VA_ARGS__)
#define LOG(level, ...) EwtsLogModule(CFE_MODULE_ID, (level), __VA_ARGS__)
#define GetLogLevel() EwtsGetLogLevelModule(CFE_MODULE_ID)
#define IsLoggingEnabled() EwtsIsLoggingEnabledModule(CFE_MODULE_ID)
```

Consumer module

``` c
#include "logger.h"

LOG(INFO, "Initializing CFE");
```

## Payload logging

C modules can write structured STATUS payloads with `PAYLOAD_STATUS(...)`:

``` c
PAYLOAD_STATUS(
    EWTS_ID_CFE,
    "INITIALIZING",
    0.1,
    "In bmi_cfe::Initialize()",
    "CFE");
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

## Environment configuration

| Variable | Purpose |
|----------|---------|
| `EWTS_USE_NGEN_BRIDGE` | Enables logging through the ngen bridge when set to a truthy value (`1`, `true`, `yes`, `on`, etc.) |
| `EWTS_ENABLED` | Enables or disables logging. |
| `EWTS_LOG_LEVEL` | Default log level; `INFO` if undefined. |
| `EWTS_RANK` | Optional MPI rank used in initialization messages. |
| `<MODULE>_LOGLEVEL` | Per-module log level override. |

## Runtime behavior

### ngen bridge

When `EWTS_USE_NGEN_BRIDGE` is enabled and the optional
`ewts_ngen_bridge` library is linked into the application, log messages
are forwarded through the ngen logging infrastructure.

STATUS payload messages are also forwarded through the bridge using
`EwtsPayloadStatus()`.

### Stdout fallback

If the ngen bridge is not enabled or is unavailable, all log messages
are written to standard output.

This provides a simple fallback for running libraries outside of ngen
while preserving the same logging API.

## MPI behavior

When running under MPI, the runtime prefixes initialization messages with the
MPI rank when `EWTS_RANK` is defined. Runtime log routing is handled by the
selected logging backend (ngen bridge or stdout fallback).

## CMakeLists.txt
Example

``` cmake
option(USE_EWTS "Build CFE with EWTS logging" ON)
message("-- CFE CMakeLists USE_EWTS = ${USE_EWTS}")
if(USE_EWTS)
    message("-- CFE compiled with EWTS logging")

    # --- EWTS (installed from nwm-ewts) ---
    find_package(ewts CONFIG REQUIRED)

    # Always use EWTS runtime logger for C
    target_link_libraries(cfebmi PRIVATE ewts::ewts_c)

    # Built with ngen bridge
    target_link_libraries(cfebmi PRIVATE ewts::ewts_ngen_bridge)
    target_compile_definitions(cfebmi PRIVATE CFE_USE_EWTS)
else()
    message("-- CFE compiled with stdout fallback logging")
endif()
```

## Related documentation

-   `integrations/ngen/README.md`
