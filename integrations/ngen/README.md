# EWTS ngen Integration

## Overview

The Error and Warning Trapping System (EWTS)  `integrations/ngen` library provides
the framework-specific logging integration used when EWTS executes within the
 **ngen** framework.

EWTS runtime libraries are designed to remain framework-independent. The ngen
integration layer provides the ngen-specific services required to support
centralized, MPI-aware logging across mixed-language applications. It allows
C, C++, Fortran, and Python components to participate in the same logging
implementation while continuing to use their native EWTS APIs.

This README focuses specifically on the ngen integration layer. For the overall
EWTS architecture, generated constants, runtime libraries, and repository
organization, see the top-level `README.md`.

---

## Responsibilities

The ngen integration layer is responsible for:

- Initializing the shared ngen logger.
- Reading and applying the `ngen_logging.json` configuration.
- Determining the effective log level for every EWTS module.
- Detecting the current MPI process rank.
- Managing unified or split log files.
- Managing the dedicated payload log.
- Providing the common bridge API used by all EWTS runtime libraries.
- Establishing the runtime environment consumed by the language runtimes.

These responsibilities are intentionally isolated from the runtime libraries so
that each runtime can operate both inside and outside of ngen.

---

## How ngen Uses the EWTS Integration

ngen uses EWTS through a compile-time build option named `USE_EWTS`.

When `USE_EWTS` is enabled, ngen includes the EWTS ngen logger and links against
the EWTS ngen bridge. When `USE_EWTS` is disabled, ngen does not link EWTS and
instead uses a small local stdout fallback logger.

This allows the same ngen source code to compile in either mode.

---

## Build-Time Integration

The ngen top-level `CMakeLists.txt` defines the `USE_EWTS` option:

```cmake
option(USE_EWTS "Build with EWTS logging support" ON)
```

When `USE_EWTS` is enabled, ngen:

1. Adds the `USE_EWTS` compile definition.
2. Finds the installed EWTS package.
3. Creates the `NGen::ewts` interface target.
4. Links the EWTS ngen bridge and C++ runtime.
5. Links ngen itself against `NGen::ewts`.

```cmake
if(USE_EWTS)
    add_compile_definitions(USE_EWTS)

    find_package(ewts CONFIG REQUIRED)

    target_link_libraries(ngen_ewts INTERFACE
        ewts::ewts_ngen_bridge
        ewts::ewts_cpp
    )

    target_link_libraries(ngen PRIVATE NGen::ewts)
endif()
```

The `partitionGenerator` target also links against `NGen::ewts` when EWTS is
enabled:

```cmake
if(USE_EWTS)
    target_link_libraries(partitionGenerator PUBLIC NGen::ewts)
endif()
```

When `USE_EWTS` is disabled, ngen records EWTS as disabled in the build summary
and compiles with stdout fallback logging.

---

## ngen Logger Header

ngen uses a single logger-facing header, `Logger.hpp`, to hide whether EWTS is
enabled.

When `USE_EWTS` is defined, `Logger.hpp` includes the EWTS ngen logger:

```cpp
#ifdef USE_EWTS
#include "ewts_ngen/logger.hpp"

inline constexpr const char* NGEN_MODULE_ID = ewts_ngen::modules::EWTS_ID_NGEN;
```

In this mode, ngen uses the EWTS-provided logger and the generated EWTS module
identifier for `NGEN`.

When `USE_EWTS` is not defined, `Logger.hpp` provides a local fallback logger:

```cpp
#else

inline constexpr const char* NGEN_MODULE_ID = "NGEN";

enum class LogLevel {
    NOTSET = 0,
    DEBUG = 10,
    INFO = 20,
    WARNING = 30,
    SEVERE = 40,
    FATAL = 50,
    STATUS = 60
};

#define LOG(...) Log(__VA_ARGS__)

#endif
```

The fallback logger writes formatted messages to stdout using a UTC timestamp,
the `NGEN` module identifier, and a simple log-level filter.

---

## ngen Logging Modes

| Build Mode | Behavior |
| --- | --- |
| `USE_EWTS=ON` | ngen includes `ewts_ngen/logger.hpp`, links the EWTS ngen bridge, and participates in centralized EWTS logging. |
| `USE_EWTS=OFF` | ngen does not link EWTS and uses the local stdout fallback logger defined in `Logger.hpp`. |

---

## Runtime Environment

During initialization, the integration layer establishes runtime state consumed
by the EWTS language runtimes.

| Environment Variable | Purpose |
| --- | --- |
| `EWTS_USE_NGEN_BRIDGE` | Indicates that runtime libraries should forward log messages through the ngen bridge. |
| `EWTS_ENABLED` | Indicates whether logging is enabled. |
| `EWTS_RANK` | Identifies the MPI process rank for the current process. |
| `<MODULE>_LOGLEVEL` | Defines the effective logging level for each EWTS module. |

Runtime libraries read these values during logger initialization and use them to
adopt the existing ngen logging environment without requiring framework-specific
logic in each language implementation.

The ngen integration layer does **not** export `EWTS_LOG_DIR`. Standalone runtime
logging may use that variable, but it is not established by the ngen integration
layer.

---

## Bridge Interface

All runtime libraries communicate with the ngen logger through a common C bridge.

Typical bridge entry points include:

```c
void ewts_ngen_log(
    const char* ewts_id,
    int level,
    const char* message);

void ewts_ngen_payload_status(
    const char* ewts_id,
    const char* status,
    double prog,
    const char* msg,
    const char* modnm);
```

The bridge provides a language-independent interface that allows the logging
implementation to remain centralized while preserving native APIs for each
supported language.

---

## Payload STATUS Handling

EWTS supports structured workflow status messages through the `STATUS` log
level. These messages are used to communicate model progress and state to the
ngen Runtime Environment (RTE) and other workflow consumers.

Under the ngen integration layer, Payload STATUS messages are handled separately
from standard diagnostic log messages.

### Payload Bridge Path

Runtime libraries forward Payload STATUS messages through the ngen bridge using
the payload-specific bridge entry point:

```c
void ewts_ngen_payload_status(
    const char* ewts_id,
    const char* status,
    double prog,
    const char* msg,
    const char* modnm);
```

This keeps Payload routing centralized in the ngen integration layer instead of
requiring each runtime library to manage payload files or ngen-specific output
rules.

### Payload Output

Payload STATUS records are written to the dedicated ngen payload log file.

Payload logs are:

- Created only when the first Payload STATUS message is emitted.
- Maintained separately from standard EWTS log files.
- Organized independently for each MPI process rank.
- Not split by module, even when standard logs are configured to split by module.
- Intended for workflow monitoring, orchestration, and RTE status reporting.

Typical payload log names include:

```text
ngen_payload_mpi_process_0.log
ngen_payload_mpi_process_1.log
```

### Payload Message Content

A Payload STATUS message typically includes:

| Field | Purpose |
| --- | --- |
| `status` | Workflow status value, such as `INITIALIZING`, `IN_PROGRESS`, `COMPLETE`, or `ERROR`. |
| `prog` | Numeric progress value. |
| `msg` | Optional human-readable status message. |
| `modnm` | Module name associated with the status update. |

Example payload content:

```text
<MSG_DATA>{"status":"INITIALIZING","prog":0.1,"msg":"Initializing CFE","modnm":"CFE"}</MSG_DATA>
```

When written by the ngen integration layer, the payload record is formatted with
the EWTS timestamp, module identifier, `STATUS` level, and structured payload
content.

Example payload log record:

```text
2026-06-23T23:42:36.210Z CFE STATUS <MSG_DATA>{"status":"INITIALIZING","prog":0.1,"msg":"Initializing CFE","modnm":"CFE"}</MSG_DATA>
```

### Difference from Standard Logging

Payload STATUS messages are not treated as normal diagnostic log messages.

| Message Type | Destination | Split by Module | Primary Consumer |
| --- | --- | --- | --- |
| Standard log message | EWTS/ngen log file | Optional | Developers and operators |
| Payload STATUS message | Dedicated payload log file | No | RTE, workflow monitoring, and automation |

This distinction allows EWTS to support both human-readable diagnostic logging
and structured workflow status reporting without mixing the two output streams.

---

## Logging Behavior

The integration layer manages log output produced while running under ngen.

| Behavior | Description |
| --- | --- |
| Unified logs | Standard log messages for a rank may be written to a shared ngen log file. |
| Split logs | Standard log messages may be separated into per-module log files, depending on the runtime logging configuration. |
| Payload logs | STATUS payload records are written to a dedicated payload log file. |
| MPI-aware output | Log output is organized independently for each MPI process rank. |
| Centralized formatting | The ngen logger owns formatting, routing, and output management when EWTS is enabled. |

Applications and runtime libraries do not manage ngen-integrated log files
directly. They forward messages through the bridge, and the integration layer
handles routing and output.

---

## Relationship to the Runtime Libraries

The EWTS runtime libraries are intentionally framework-independent.

When the ngen integration establishes `EWTS_USE_NGEN_BRIDGE`, runtime libraries
automatically forward log messages through the shared bridge and participate in
the centralized logging implementation.

When the integration layer is not present, each runtime falls back to its own
standalone logging implementation as described in its respective runtime README.

This separation allows the same runtime libraries to be used both within ngen
and in standalone applications without changes to application code.

---

## Build Summary Output

When configuring ngen, the CMake build summary reports whether EWTS is enabled
and which EWTS package version was found.

Example summary fields include:

```text
EWTS:
  Use EWTS: ON
  Package Version: <version>
  NGWPC Version: <version>
```

When EWTS is disabled, these values are reported as disabled.

---

## See Also

- Top-level `README.md` — overall EWTS architecture and repository organization.
- `runtime/c/README.md` — C runtime and standalone C logging behavior.
- `runtime/cpp/README.md` — C++ runtime.
- `runtime/fortran/README.md` — Fortran runtime.
- `runtime/python/README.md` — Python runtime.
