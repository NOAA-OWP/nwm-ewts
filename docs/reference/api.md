# API Reference

This reference summarizes the stable concepts exposed by EWTS runtimes. Exact names may vary by language binding or local wrapper.

## Log levels

| Name | Value | Purpose |
| --- | ---: | --- |
| `NOTSET` | 0 | No explicit level. |
| `DEBUG` | 10 | Debug diagnostics. |
| `PERFORM` | 15 | Performance/timing messages. |
| `INFO` | 20 | Informational messages. |
| `WARNING` | 30 | Warning messages. |
| `SEVERE` | 40 | Severe error conditions. |
| `FATAL` | 50 | Fatal error conditions. |
| `STATUS` | 60 | Conventional level used for payload messages; emitted even when diagnostic filtering would suppress lower levels. |

## Payload statuses

| Status | JSON value | Meaning |
| --- | --- | --- |
| `NULL` | `NULL` | No specific state. |
| `INITTING` / `INITIALIZING` | `INITIALIZING` | Payload is initializing. |
| `INITTED` / `INITIALIZED` | `INITIALIZED` | Payload initialization completed. |
| `STARTING` | `STARTING` | Payload execution is starting. |
| `INPROG` / `IN_PROGRESS` | `IN_PROGRESS` | Payload work is in progress. |
| `COMPLETE` | `COMPLETE` | Payload completed successfully. |
| `ERROR` | `ERROR` | Payload encountered an error state. |

## Runtime boundaries

| Runtime or layer | Primary audience |
| --- | --- |
| Python runtime | Python modules and components. |
| C runtime | C modules and ABI-friendly native integration. |
| C++ runtime | C++ model modules. |
| Fortran runtime | Fortran model modules and wrappers. |
| ngen bridge | ngen integration layer that forwards EWTS messages into the ngen logging path. |
