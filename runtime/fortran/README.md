
# EWTS Fortran Runtime

The The Error and Warning Trapping System (EWTS) Fortran runtime provides 
logging support for Fortran modules. When linked with the optional ngen 
bridge, log messages are forwarded into the ngen logging infrastructure. 
Otherwise, log messages are written directly by the Fortran runtime, using 
the directory specified by the `EWTS_LOG_DIR` environment variable when 
available, or stdout if no usable log directory is configured.

It supports two logging modes:

- **ngen-integrated logging**, where messages are forwarded through the optional ngen bridge.
- **standalone logging**, where messages are written directly by the CPP runtime without the ngen bridge.

The runtime shares the same logging model, log levels, payload format, and
environment-driven configuration as the C, C++, and Python EWTS runtime
libraries.

## Directory role

The implementation in `runtime/fortran/` provides:

- Native Fortran logger implementation.
- Generated EWTS module IDs, log level, and payload status constants.
- Module-aware logging APIs.
- Optional bridge support for `ngen`.

## Design goals

The Fortran runtime is designed to provide:

- Consistent behavior across all EWTS runtime libraries.
- Module-scoped logging.
- Environment-driven configuration.
- Transparent forwarding to the ngen logging infrastructure.
- Stdout fallback outside ngen.

## Shared-runtime behavior

Multiple modules may execute within the same process. Each logger is therefore
associated with a specific EWTS module ID rather than being process-global,
ensuring that log messages are correctly attributed regardless of the execution
environment.

## Public API

```fortran
call logger_init(id)
call logger_init_module(id)

call write_log(msg, lvl)
call write_log_module(id, msg, lvl)

enabled = is_logger_enabled()
enabled = is_logger_enabled_module(id)

lvl = get_log_level()
lvl = get_log_level_module(id)

call payload_status(ewts_id, status, prog, msg, modnm)
```

Prefer the module-specific APIs so log messages are attributed to the correct
EWTS module ID.

## Log levels

| Level | Value |
|-------|------:|
| `EWTS_NOTSET` | 0 |
| `EWTS_DEBUG` | 10 |
| `EWTS_PERFORM` | 15 |
| `EWTS_INFO` | 20 |
| `EWTS_WARNING` | 30 |
| `EWTS_SEVERE` | 40 |
| `EWTS_FATAL` | 50 |
| `EWTS_STATUS` | 60 |

`EWTS_STATUS` is reserved for structured payload messages.

## Standard logging

```fortran
use logger
use ewts_module_constants

call logger_init_module(EWTS_ID_NOAH_OWP_MODULAR)

call write_log_module( &
    EWTS_ID_NOAH_OWP_MODULAR, &
    "Initializing NOAHOWP BMI", &
    EWTS_INFO)
```

Initialize the logger before writing the first log message.

## Payload logging

```fortran
call payload_status( &
    EWTS_ID_NOAH_OWP_MODULAR, &
    PAYLOAD_INITTING, &
    0.1d0, &
    "Initializing NOAHOWP BMI", &
    "NOAHOWP")
```

| Argument | Meaning |
|----------|---------|
| `ewts_id` | EWTS ID used in the payload log prefix. |
| `status` | Payload status such as `INITIALIZING` or `IN_PROGRESS`. |
| `prog` | Progress value, typically `0.0d0` through `1.0d0`. |
| `msg` | Human-readable payload message. |
| `modnm` | Module/component name written into the JSON payload. |

## Payload status constants

| Constant | String Value | Description |
|----------|--------------|-------------|
| `PAYLOAD_NULL` | `"NULL"` | No status assigned. |
| `PAYLOAD_INITTING` | `"INITIALIZING"` | Module initialization in progress. |
| `PAYLOAD_INITTED` | `"INITIALIZED"` | Module initialization complete. |
| `PAYLOAD_STARTING` | `"STARTING"` | Module execution beginning. |
| `PAYLOAD_INPROG` | `"IN_PROGRESS"` | Module execution in progress. |
| `PAYLOAD_COMPLETE` | `"COMPLETE"` | Module execution completed successfully. |
| `PAYLOAD_ERROR` | `"ERROR"` | Module execution terminated with an error. |

Payload logging is active only when `EWTS_USE_NGEN_BRIDGE` is enabled and the
optional bridge function is available. Otherwise, `payload_status()` performs
no action.

## Submodule wrapper pattern

The recommended integration pattern is to provide a small module-specific wrapper
that:

- Imports EWTS only when the model is built with EWTS support.
- Maps local log-level constants to EWTS constants.
- Initializes EWTS once before the first log message.
- Forwards log messages using `write_log_module()`.
- Forwards payload messages using `payload_status()`.
- Falls back to simple stdout logging when EWTS is not enabled.

For an example wrapper implementation, see `noah-owp-modular/src/noahowpLogger.f90`

## Runtime relationship to `ngen`

When `EWTS_USE_NGEN_BRIDGE` is enabled and the optional bridge library is
available, log messages and STATUS payloads are forwarded to the ngen logging
infrastructure.

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

When the bridge is enabled, log messages and payloads are forwarded through the
ngen logging infrastructure.

### Stdout fallback

If the bridge is not enabled or unavailable, log messages are written to
standard output. Payload logging becomes a no-op.

## MPI behavior

Initialization messages are prefixed with the MPI rank when `EWTS_RANK` is
defined. Runtime log routing is handled by the selected backend.

## CMakeLists.txt

Example

```cmake
option(USE_EWTS "Build NOAHOWP with EWTS logging" ON)
message("-- NOAHOWP CMakeLists USE_EWTS = ${USE_EWTS}")
if(USE_EWTS)
    message("-- NOAHOWP compiled with EWTS logging")

    # --- EWTS (installed from nwm-ewts) ---
    find_package(ewts CONFIG REQUIRED)

    # Always use EWTS runtime logger for Fortran
    target_link_libraries(surfacebmi PRIVATE ewts::ewts_fortran)

    # Built with ngen bridge
    target_link_libraries(surfacebmi PRIVATE 
        ewts::ewts_ngen_bridge
    )
    target_compile_definitions(surfacebmi PRIVATE NOAHOWP_USE_EWTS)
else()
    message("-- NOAHOWP compiled with stdout fallback logging")
endif()
```

## Related documentation

- `runtime/c/README.md`
- `runtime/cpp/README.md`
- `runtime/python/README.md`
- `integrations/ngen/README.md`
