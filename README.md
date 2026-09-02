# Error and Warning Trapping System (EWTS)

The Error and Warning Trapping System (EWTS) is a cross-language logging and status
reporting framework designed for hydrologic and environmental modeling
applications. It provides a common logging architecture for Python, C, C++, and
Fortran while preserving the conventions and programming style of each language.

Unlike traditional logging libraries, EWTS is specification-driven. Common
definitions such as module identifiers, log levels, and payload status values
are maintained in a single location and automatically generate the language-
specific constants consumed by every runtime library or Python package. This eliminates duplicated
definitions and keeps every supported language synchronized.

EWTS is currently designed to integrate with the ngen framework, providing MPI-aware logging, per-module log level configuration, structured Payload reporting, and consistent behavior across mixed-language applications. Within the ngen integration, all modules log through a common ngen integration bridge, which provides a language-independent interface to the ngen logging infrastructure. This establishes a **single source of truth** for log formatting and log file management, with the ngen logger responsible for all formatting and output. In MPI executions, log output is generated independently by each MPI process rank and may be written either to a single unified log file or to separate per-module log files, depending on the runtime logging configuration.


## Table of Contents

-   [Introduction](#introduction)
-   [Features](#features)
-   [Logging Overview](#logging-overview)
    -   [Log Levels](#log-levels)
    -   [Payload Statuses](#payload-statuses)
    -   [Log Output](#log-output)
        -   [EWTS Log Files](#ewts-log-files)
        -   [Payload Log Files](#payload-log-files)
-   [ngen Integration](#ngen-integration)
-   [Building ngen with EWTS](#building-ngen-with-ewts)
    -   [Building EWTS](#building-ewts)
    -   [Enabling EWTS Support](#enabling-ewts-support)
    -   [Compile-Time Control](#compile-time-control)
    -   [Python Components](#python-components)
-   [Architecture](#architecture)
    -   [Repository Organization](#repository-organization)
    -   [Specification-Driven Design](#specification-driven-design)
    -   [Code Generation Overview](#code-generation-overview)
    -   [Common Logging Model](#common-logging-model)
    -   [Runtime Libraries](#runtime-libraries)
        -   [Python Runtime](#python-runtime)
        -   [C Runtime](#c-runtime)
        -   [C++ Runtime](#c-runtime)
        -   [Fortran Runtime](#fortran-runtime)
    -   [Language APIs](#language-apis)
    -   [Framework Integration](#framework-integration)
    -   [Configuration](#configuration)
-   [Documentation Layout](#documentation-layout)
-   [Extending EWTS](#extending-ewts)
-   [Summary](#summary)


------------------------------------------------------------------------

------------------------------------------------------------------------

## Introduction

EWTS provides a common infrastructure for logging debug, information, errors,
warnings and fatal messagse as well as performance logging, and structured 
workflow status messages.

The project is organized around a simple principle:

> Define shared concepts once, generate language-specific interfaces
> automatically, and provide consistent runtime behavior regardless of language.

Instead of maintaining separate copies of constants and identifiers for each
runtime library, EWTS generates those definitions from common specifications.
Every runtime therefore shares identical module identifiers, log levels, and
payload status values.

---

## Features

-   C, C++, and Fortran runtime libraries
-   Python pacakge
-   Common log levels across all languages
-   Configurable module log levels
-   MPI-aware logging
-   Unified or split log files
-   ngen integration
-   Structured Payload messages
-   Dedicated payload log files
-   Environment-based configuration
-   External workflow and monitoring support

------------------------------------------------------------------------

## Logging Overview

### Log Levels

| Level | Description | Typical Use |
|-------|-------------|-------------|
| `DEBUG` | Provides detailed diagnostic information useful for developers during development and troubleshooting. | Internal variable values, function entry/exit points, iteration steps, or other low-level system details. |
| `INFO` | Records general application events that confirm expected operations are occurring. | Startup/shutdown notifications, configuration summaries, successful task completions. |
| `WARNING` | Indicates a potential problem or unexpected situation that does not prevent the application from continuing to operate. | Deprecated API usage, missing optional files, repeatable errors. |
| `SEVERE` | Signals a significant problem that could impact program execution, but the application may still continue in a degraded state. | Failed external service connections, corrupted configuration, partial data loss. |
| `FATAL` | Indicates a critical failure that causes the application to abort or enter an unrecoverable state. | Application crashes, unrecoverable memory errors, invalid system state. |


Example log:

``` text
std::stringstream ss;
ss << "Initializing formulations" << std::endl;
LOG(ss.str(), LogLevel::INFO);
```

------------------------------------------------------------------------

### Payload Statuses

| Status | Purpose |
|------|---------|
| `NULL` | Emits json null. Possible heartbeat message. |
| `INITIALIZING` | Module starting initialization |
| `INTIALIZED` | Moduled completed initialization |
| `STARTING` | Module starting its simulation |
| `IN_PROGRESS` | In progress status message |
| `COMPLETE` | Module has completed execution |
| `ERROR` | Module reporting an error |

The STATUS log level provides structured status and progress
information.

Example payload:

``` text
<MSG_DATA>
{
    "status": "INITIALIZING",
    "prog": 0.1,
    "msg": "Initializing UEB",
    "modnm": "ueb_bmi"
}
</MSG_DATA>
```

### Log Output

EWTS produces two complementary forms of log output. Standard log files record human-readable diagnostic information intended for developers and system operators, while Payload log files capture structured workflow status information for monitoring systems, orchestration frameworks, and other automated consumers. Both forms of output are generated independently for each MPI process rank and together provide a comprehensive view of application execution for both interactive troubleshooting and automated workflow management.

#### EWTS Log Files

EWTS log files are created when the first standard log message is written. These logs are primarily intended for human consumption, providing a chronological record of application execution, diagnostics, warnings, and errors. Because the log format is consistent and structured, the files may also be processed by automated tools for analysis, monitoring, or post-processing.

Example:

`2026-06-26T23:15:21.023Z NGEN     INFO    Initializing formulations`

Typical file names:

Unified Logs:
``` text
ngen_mpi_process_0.log
ngen_mpi_process_1.log
```

Split by Module Logs:
``` text
ngen_mpi_process_0.log
ngen_mpi_process_1.log
forcing_mpi_process_0.log
forcing_mpi_process_1.log
cfe_mpi_process_0.log
cfe_mpi_process_1.log
noahowp_mpi_process_0.log
noahowp_mpi_process_1.log
troute_mpi_process_0.log
troute_mpi_process_1.log
```

#### Payload Log Files
When running under the ngen framework, Payload messages are written to a dedicated payload log file. The file is created when the first payload is emitted. Payload logs are maintained independently for each MPI process rank and are never split by module. Unlike standard EWTS log files, payload logs are intended primarily for machine consumption by workflow orchestration, monitoring, and external automation systems, while remaining fully human-readable for inspection and debugging.

Example:

``` text
2026-06-23T23:42:36.210Z UEB_BMI STATUS <MSG_DATA>{"status":"INITIALIZING","prog":0.1,"msg":"Initializing UEB","modnm":"ueb_bmi"}</MSG_DATA>
```

Typical file names:

``` text
ngen_payload_mpi_process_0.log
ngen_payload_mpi_process_1.log
```

## ngen Integration

The EWTS runtime libraries are framework-independent and may be used in standalone applications. The ngen integration builds upon those runtime libraries by providing framework-specific services that enable unified logging across mixed-language applications executing under ngen.

The integration layer provides:

-   Runtime and environment-based configuration
-   MPI process rank detection
-   A common ngen logging bridge
-   Module-aware logging
-   Payload routing
-   Per-module log level configuration
-   Unified or split module log files
-   Dedicated Payload log files (never split by module)

All C, C++, Fortran, and Python components log through a common ngen integration bridge, which provides a language-independent interface to the ngen logger. This establishes a centralized implementation for log formatting, routing, and file management while preserving the originating EWTS module identifier.

Applications continue to use the native EWTS APIs while the integration layer transparently manages routing, configuration, MPI-aware log organization, and Payload handling.

```text
            Applications
                 |
                 v
        EWTS Language APIs
                 |
                 v
       ngen Integration Layer
    +--------------------------+
    | Unified Logs             |
    | Split Logs               |
    | Payload Logs             |
    | MPI Support              |
    +------------+-------------+
                 |
                 v
          Log Consumers
```

---

## Building ngen with EWTS

EWTS is designed to operate as an optional integration within the ngen framework. This section describes the build system used by EWTS, how EWTS support is enabled or disabled when building ngen, and how compile-time and runtime configuration control the behavior of participating components.

### Building EWTS

EWTS uses CMake to build its runtime libraries and supporting integration components. The repository is organized so that language-specific runtime libraries may be built independently, while integration components provide optional framework-specific capabilities such as support for the ngen logging infrastructure.

Generated source files are treated as build artifacts derived from the EWTS specifications and should not be edited manually. Whenever the specification files are modified, the language-specific bindings should be regenerated before rebuilding the affected runtime libraries.

### Enabling EWTS Support

Integration of EWTS with the ngen framework is optional and controlled at build time through the top-level `USE_EWTS` CMake option. This option determines whether EWTS support is compiled into `ngen` and its supported submodules and components. When EWTS support is disabled, participating components automatically fall back to writing log messages to standard output (`stdout`).

``` text
-DUSE_EWTS=ON
```

or

``` text
-DUSE_EWTS=OFF
```

If `USE_EWTS` is not specified, it defaults to `ON`.

#### USE_EWTS=ON

When `USE_EWTS=ON`:

-   EWTS libraries are linked into ngen and participating submodules.
-   Language-specific preprocessor definitions are enabled.
-   Structured Payload messages are available.
-   STATUS messages are written to dedicated payload log files.
-   Module log messages are written through the EWTS framework.
-   ngen components may participate in unified MPI-aware logging.

#### USE_EWTS=OFF

When `USE_EWTS=OFF`:

-   EWTS libraries are not linked.
-   EWTS-related code paths are excluded at compile time.
-   All logging falls back to standard stdout logging.
-   Payload messages are not generated.
-   Payload log files are not created.

### Compile-Time Control

Each ngen component controls EWTS support through its own build
configuration.

#### C, C++, and Fortran Modules
C and C++ modules use preprocessor definitions generated by their
`CMakeLists.txt` files, for example:

``` cmake
if(USE_EWTS)
    target_compile_definitions(cfebmi PRIVATE CFE_USE_EWTS)
endif()
```

Fortran components similarly define module-specific compilation flags
such as:

``` text
NOAHOWP_USE_EWTS
SACSMA_USE_EWTS
SNOW17_USE_EWTS
```

These definitions allow EWTS-specific code to be completely excluded
from builds when EWTS support is disabled.

#### Python Modules

Python modules determine EWTS availability at runtime.

The EWTS Python package must be importable:

``` python
try:
    from ewts.helper import getenv_any
    from ewts.logger import configure_existing_logger
    FORCING_USE_EWTS = True
except ImportError:
    FORCING_USE_EWTS = False
```

If the package is available, the component checks the
`EWTS_USE_NGEN_BRIDGE` environment variable.

When both conditions are satisfied:

-   The existing ngen logger is adopted.
-   Log messages participate in the ngen EWTS framework.
-   Payload messages may be generated.

If the package is unavailable, or if `EWTS_USE_NGEN_BRIDGE` is not
enabled, Python components automatically fall back to standard stdout
logging.

This allows the same Python component to operate both inside and outside
of ngen without requiring code changes.

---

## Arichecture

### Repository Organization

```
spec/
    module_registry.yaml
    log_levels.json
    payload_status.json

tools/
    generate_language_constants.py

runtime/
    python/
    c/
    cpp/
    fortran/

integrations/
    ngen/

docs/
```

Each directory has a distinct responsibility:

- **spec** contains the authoritative definitions shared by every runtime.
- **tools** contains the code generator.
- **runtime** contains the language-specific implementations.
- **integrations** contains framework-specific adapters.
- **docs** contains the published documentation.

---

### Specification-Driven Design

EWTS intentionally separates **definitions** from **implementations**.

Instead of manually maintaining constants in multiple languages, EWTS stores
shared definitions in specification files.

Current specifications include:

| Specification | Purpose |
|--------------|---------|
| `module_registry.yaml` | Defines every EWTS module identifier and registry metadata. |
| `log_levels.json` | Defines canonical logging levels shared by every runtime. |
| `payload_status.json` | Defines workflow status values used by structured Payloads. |

These specifications are the authoritative source for every runtime library.

Any change to a specification is propagated simply by regenerating the language
bindings.


---

### Code Generation Overview

The `tools/generate_language_constants.py` utility generates language-specific
artifacts directly from the specifications.

Rather than defining constants independently within each runtime, EWTS generates
language-specific definitions directly from the specification files.

Generated artifacts include:

- Module identifiers
- Log levels
- Payload status values

Examples include:

- C headers
- C++ headers
- Fortran modules
- Python constants (where applicable)
- ngen integration headers

This approach provides several advantages:

- One authoritative definition.
- No duplicated constants.
- Consistent naming across languages.
- Easier maintenance.
- Simplified addition of new modules and log levels.
- Guarantees that every runtime consumes identical definitions while allowing each language to present them using native conventions.

---

### Common Logging Model

One of the primary design goals of EWTS is that an application developer should
not need to think differently about logging simply because a component is
written in another programming language. Every runtime library and the Python
package implement the same conceptual logging model while exposing APIs that are
idiomatic for their respective languages.

```text
                 Application
                       |
                       v
            Language-specific API
                       |
        +--------------+--------------+
        |                             |
        v                             v
  Standard Log                    Payload
        |                             |
        +--------------+--------------+
                       |
                       v
                 EWTS Output
```

Applications produce two categories of output:

- Conventional log messages intended primarily for developers and operators.
- Structured Payloads intended for monitoring systems, workflow
  orchestration, and other automated consumers.

Regardless of the originating language, EWTS applies the same logging rules,
module identifiers, configuration, and output formatting.

---

### Runtime Libraries

EWTS provides native runtime libraries for Python, C, C++, and Fortran. Each
runtime exposes an idiomatic API while implementing the same logging model,
module identifiers, log levels, and Payload semantics.

The runtime libraries are intentionally thin. Their primary responsibility is to
present a natural interface for the language while relying on generated
definitions to ensure consistency across the entire framework.

#### C Runtime

The C runtime library provides a procedural interface suitable for C applications and
libraries. Constants are generated as preprocessor macros and the logging API is
designed to have minimal runtime overhead.

The C runtime supports:

- Standard logging
- Payload logging
- Generated module identifiers
- Generated payload status values

Example Code:

``` c
LOG(INFO, "Initializing");

PAYLOAD_STATUS(
    "INITIALIZING",
    0.1,
    "",
    "CFE");
```

#### C++ Runtime

The C++ runtime library builds upon the C runtime while providing a more natural C++
interface through namespaces, overloads, and strongly typed log levels.

The C++ runtime additionally provides:

- Type-safe logging APIs
- Native `std::string` support
- Payload helpers
- Integration with the ngen bridge

Example Code:

``` cpp
LOG(INFO, "Initializing");

PAYLOAD_STATUS(
    "INITIALIZING",
    0.1,
    "",
    "NGEN");
```

#### Fortran Runtime

The Fortran runtime library exposes module procedures that closely resemble traditional
Fortran logging interfaces.

Generated modules provide common log levels, payload status values, and module
identifiers while shielding application code from implementation details.


Example Code:

``` fortran
call write_log(
    "Initializing model",
    LOG_LEVEL_INFO)

call payload_status(
    "INITIALIZING",
    0.1d0,
    "",
    "SACSMA")
```

#### Python Runtime

The Python runtime package provides the highest-level interface. It integrates with the
standard `logging` package while adding EWTS-specific capabilities including
module identifiers, Payload logging, and ngen bridge support.

The Python package includes:

- Logger management
- STATUS helper methods
- Generated log levels
- Generated module identifiers
- Runtime configuration
- ngen bridge adoption

Python applications use the standard EWTS logger.

``` python
LOG.info("Initializing")

LOG.status(
    '<MSG_DATA>'
    '{"status":"INITIALIZING",'
    '"prog":0.1,'
    '"msg":"Initializing",'
    '"modnm":"TROUTE"}'
    '</MSG_DATA>'
)
```

### Language APIs

Although each runtime follows the conventions of its language, the APIs expose
the same capabilities.

| Capability | Python | C | C++ | Fortran |
|-----------|--------|---|-----|----------|
| Standard logging | ✓ | ✓ | ✓ | ✓ |
| Payload logging | ✓ | ✓ | ✓ | ✓ |
| Generated module IDs | ✓ | ✓ | ✓ | ✓ |
| Generated log levels | ✓ | ✓ | ✓ | ✓ |
| Generated payload statuses | Native enum | Generated | Generated | Generated |

Each runtime therefore offers a familiar programming experience without
requiring developers to learn different logging concepts for different
languages.

### Framework Integrations

EWTS is designed so that runtime libraries remain independent of any particular
application framework while allowing optional integrations to provide additional
capabilities. Integrations reuse the common logging model and generated
definitions without changing the APIs exposed to application code.

The intial EWTS **ngen** framework provides a unified logging framework for mixed-language hydrologic models and adds 
capabilities beyond the runtime libraries, including:

- Unified logging across C, C++, Fortran, and Python components.
- MPI-aware logging.
- Per-module log level configuration.
- Optional split log files by module.
- Dedicated Payload log files.
- Adoption of existing Python loggers.
- Consistent module identity across all supported languages.

### Configuration

EWTS supports both compile-time and runtime configuration.

Compile-time configuration enables or disables optional integrations and bridge
libraries.

Runtime configuration controls behavior such as:

- Global log level
- Per-module log levels
- Log destinations
- Split versus unified logging
- Payload logging
- Environment-driven overrides

This separation allows the same application binaries to operate in different
deployment environments without recompilation.

---

## Documentation

The repository documentation is organized by audience.

| Document | Purpose |
|----------|---------|
| Top-level `README.md` | EWTS architecture and project overview |
| `runtime/python/README.md` | Python runtime API |
| `runtime/c/README.md` | C runtime API |
| `runtime/cpp/README.md` | C++ runtime API |
| `runtime/fortran/README.md` | Fortran runtime API |
| `integrations/ngen/README.md` | Using EWTS within ngen |

This organization keeps architectural concepts separate from language-specific
usage while allowing integrations to focus on practical deployment guidance.

---

## Extending EWTS

EWTS was designed to simplify the addition of new capabilities.

Typical extensions include:

- New module identifiers.
- Additional log levels.
- New payload status values.
- Additional runtime libraries.
- New framework integrations.

Most extensions begin by modifying the specification files. The code generator
then propagates those changes consistently to every supported language.

This specification-driven approach minimizes duplicated effort while helping
maintain behavioral consistency across the framework.


---

## Summary

EWTS provides a common logging and workflow status framework for mixed-language
scientific applications.

Its architecture is centered on three principles:

1. Define shared concepts once using language-independent specifications.
2. Generate language-specific definitions automatically.
3. Present native APIs while preserving common behavior across every runtime.

This architecture enables applications and frameworks such as ngen to share a
consistent logging infrastructure while allowing each programming language to
retain familiar development patterns.

For language-specific programming examples, refer to the runtime READMEs. For
details on integrating EWTS with ngen, see `integrations/ngen/README.md`.
