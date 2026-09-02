# EWTS Python Runtime

The EWTS Python runtime provides the Python package implementation of the NWM Error and Warning Trapping System (EWTS). It supports standalone Python logging, logging through the `ngen` EWTS bridge when available, and structured status/data payloads embedded in EWTS log messages.

The runtime shares the same logging model, log levels, payload format, and
environment-driven configuration as the C, C++, and Fortran EWTS runtime
libraries.

The package is installed and imported as:

```python
import ewts
```

The Python package source lives in:

```text
runtime/python/ewts/src/ewts
```

The Python package project root is:

```text
runtime/python/ewts
```

## Current package behavior

The current Python logger no longer uses lazy binding or bound logger proxies. `get_logger()` returns a cached, initialized `EwtsLogger` for the requested EWTS module id. `setup_logger()` resets any existing logger for the module, applies runtime overrides, and returns a freshly initialized `EwtsLogger`.

Use this model for new code:

```python
import ewts

LOG = ewts.get_logger(ewts.T_ROUTE_ID)
LOG.info("Starting routing")
```

For component startup code that needs to control the level, destination, file name, `ngen` behavior, or enabled state, use `setup_logger()`:

```python
import ewts

LOG = ewts.setup_logger(
    ewts.CAL_MGR_ID,
    level="INFO",
    log_dir="/path/to/run_logs",
    log_file_name="cal_mgr.log",
    running_in_ngen=False,
    enabled=True,
)

LOG.info("Calibration manager initialized")
```

## Installation

### Editable install for development

From the top of the EWTS repository:

```bash
python -m pip install -e runtime/python/ewts
```

### Build a distribution

```bash
python -m build runtime/python/ewts
```

### Install a built wheel

```bash
python -m pip install runtime/python/ewts/dist/ewts-<version>-py3-none-any.whl
```

### Install from Git in a consuming Python repository

A separate Python repository does not automatically gain access to EWTS just because the top-level EWTS repository was built elsewhere. The consuming Python environment still needs to install the EWTS wheel or package.

```bash
python -m pip install \
  "ewts @ git+https://github.com/NGWPC/nwm-ewts.git@development#subdirectory=runtime/python/ewts"
```

Then consuming code can import EWTS normally:

```python
import ewts
```

### Docker install example

```dockerfile
ARG GH_ORG=NGWPC
ARG EWTS_REF=development
ARG EWTS_CACHE_BUST=0

RUN --mount=type=cache,target=/root/.cache/pip,id=pip-cache \
    set -eux && \
    echo "EWTS cache bust: ${EWTS_CACHE_BUST}" && \
    ewts_dir="$(mktemp -d)" && \
    git clone "https://github.com/${GH_ORG}/nwm-ewts.git" "${ewts_dir}" && \
    cd "${ewts_dir}" && \
    git checkout "${EWTS_REF}" && \
    python -m pip install "${ewts_dir}/runtime/python/ewts" && \
    rm -rf "${ewts_dir}"
```

## Public API

The main public imports are re-exported from `ewts`:

```python
from ewts import (
    EwtsLogger,
    get_logger,
    setup_logger,
    reset_logger,
    configure_existing_logger,
    LogParts,
    Payload,
    Status,
    parts_of_log_line,
    payload_of_log_msg,
)
```

EWTS also re-exports the generated module id constants, such as:

```python
ewts.FORCING_ID        # "FORCING"
ewts.LSTM_ID           # "LSTM"
ewts.TOPOFLOW_GLACIER_ID  # "TFGLACR"
ewts.T_ROUTE_ID        # "TROUTE"
ewts.MSW_MGR_ID        # "MSWMGR"
ewts.CAL_MGR_ID        # "CALMGR"
ewts.EVAL_MGR_ID       # "EVALMGR"
ewts.FCST_MGR_ID       # "FCSTMGR"
ewts.RTE_ID            # "RTE"
ewts.ASSIM_ENGINE_ID   # "ASSIM"
```

## Logging levels

EWTS defines these canonical levels:

| Name | Value |
|---|---:|
| `NOTSET` | 0 |
| `DEBUG` | 10 |
| `PERFORM` | 15 |
| `INFO` | 20 |
| `WARNING` | 30 |
| `SEVERE` | 40 |
| `FATAL` | 50 |
| `STATUS` | 60 |

The Python runtime registers `PERFORM` and `STATUS` with the standard `logging` package and adds `logging.Logger.perform()` and `logging.Logger.status()` when they are not already present.

## Basic logging examples

### Get a module logger

```python
import ewts

LOG = ewts.get_logger(ewts.T_ROUTE_ID)
LOG.debug("debug detail")
LOG.perform("performance message")
LOG.info("normal message")
LOG.warning("warning message")
LOG.severe("severe message")
LOG.fatal("fatal message")
LOG.status("status message")
```

`error()` is an alias for `severe()`, and `critical()` is an alias for `fatal()`.

### Configure a logger explicitly

```python
import ewts

LOG = ewts.setup_logger(
    ewts.FCST_MGR_ID,
    enabled=True,
    level="DEBUG",
    log_dir="./run_logs",
    log_file_name="forecast_manager.log",
    running_in_ngen=False,
)

LOG.info("Forecast manager ready")
```

`setup_logger()` accepts either a string level name or integer level. Unknown string values default to `INFO`.

### Configure an existing Python logger

Use `configure_existing_logger()` when a component already creates a standard `logging.Logger` and you want EWTS to manage its output. The logger name must be a known EWTS module id, such as `TROUTE`, `CALMGR`, or `FCSTMGR`.

```python
import logging
import ewts

LOG = logging.getLogger("TROUTE")
ewts.configure_existing_logger(LOG)

LOG.info("This standard Python logger is now routed through EWTS")
LOG.status("Routing status update")
```

If the logger name is not a known EWTS module id, `configure_existing_logger()` raises `ValueError`.

## Runtime configuration

EWTS can be configured with environment variables or through `setup_logger()` overrides.

| Variable | Purpose |
|---|---|
| `EWTS_ENABLED` | Enables/disables logging. Defaults to enabled. False values: `0`, `false`, `off`, `no`. |
| `EWTS_LOG_LEVEL` | Default EWTS log level. Defaults to `INFO`. |
| `<EWTS_ID>_LOGLEVEL` | Per-module log level override, for example `TROUTE_LOGLEVEL` or `CALMGR_LOGLEVEL`. |
| `EWTS_LOG_DIR` | Standalone log output directory. If unset, the standalone logger writes to stdout. |
| `EWTS_RANK` | MPI rank. When set, the rank is included in initialization output and file naming. |
| `EWTS_USE_NGEN_BRIDGE` | Indicates that EWTS should try to route logging through the `ngen` bridge. |
| `EWTS_NGEN_BRIDGE_LIB` | Optional explicit path to the EWTS `ngen` bridge shared library. |
| `EWTS_DEBUG` | Prints bridge load errors when bridge loading fails. |

`setup_logger()` overrides are applied for the requested EWTS id and take precedence over environment-derived values for that logger.

## Standalone logging behavior

When `EWTS_USE_NGEN_BRIDGE` is not set, or when the `ngen` bridge cannot be loaded, EWTS uses standalone logging.

If `EWTS_LOG_DIR` or `setup_logger(log_dir=...)` is set, EWTS writes log lines to a file under that directory. If no log directory is provided, EWTS writes to stdout.

Standalone log lines use this prefix format:

```text
<UTC timestamp> <EWTS_ID padded to 8 chars> <LEVEL padded to 7 chars> <message>
```

Example:

```text
2026-06-18T18:13:15.123Z TROUTE   INFO    Starting routing
```

## `ngen` bridge behavior

When `EWTS_USE_NGEN_BRIDGE` is set, EWTS attempts to load the `ngen` bridge and send messages through:

```text
ewts_ngen_log(const char* ewts_id, int level, const char* message)
```

Bridge loading uses this order:

1. `EWTS_NGEN_BRIDGE_LIB`, if set.
2. `libewts_ngen_bridge.so` from the runtime loader path.

If the bridge cannot be loaded, EWTS falls back to standalone logging instead of failing the component.

## Resetting a logger

Use `reset_logger()` in tests or reconfiguration paths when a logger should be rebuilt from new environment variables or new runtime overrides.

```python
import ewts

ewts.reset_logger(ewts.T_ROUTE_ID)
LOG = ewts.get_logger(ewts.T_ROUTE_ID)
```

`reset_logger()` clears the cached EWTS logger, resets the internal `ewts.<ID>` Python logger, and also resets a same-named application logger such as `TROUTE` when `configure_existing_logger()` was used.

## Structured status/data payloads

The `ewts.data_payloads` module supports structured status reporting by embedding a JSON payload inside a normal EWTS log message. The payload is wrapped with sentinel strings so it can be found and parsed later from a log line.

The sentinel strings are:

```text
<MSG_DATA>
</MSG_DATA>
```

A payload is represented by the `Payload` dataclass:

```python
@dataclass
class Payload:
    status: Status
    prog: float | None = None
    msg: str | None = None
    modnm: str | None = None
```

### Payload status values

`Status` is a `StrEnum` with these values:

| Enum | JSON value |
|---|---|
| `Status.NULL` | `NULL` |
| `Status.INITTING` | `INITIALIZING` |
| `Status.INITTED` | `INITIALIZED` |
| `Status.STARTING` | `STARTING` |
| `Status.INPROG` | `IN_PROGRESS` |
| `Status.COMPLETE` | `COMPLETE` |
| `Status.ERROR` | `ERROR` |

### Creating and logging a payload

```python
import ewts
from ewts import Payload, Status

LOG = ewts.get_logger(ewts.T_ROUTE_ID)

payload = Payload(
    status=Status.INPROG,
    prog=0.50,
    msg="Routing is 50% complete",
    modnm="t-route",
)

LOG.status(payload)
```

Because `Payload.__str__()` returns the JSON wrapped with the sentinel strings, the log message includes text like:

```text
<MSG_DATA>{"status": "IN_PROGRESS", "prog": 0.5, "msg": "Routing is 50% complete", "modnm": "t-route"}</MSG_DATA>
```

You can also include a payload in a larger message:

```python
LOG.status("status update: %s", payload)
```

### Payload validation

`Payload` validates its fields when constructed:

- `status` must be a `Status` enum value.
- `prog` must be `None` or a `float` between `0.0` and `1.0`.
- `msg` must be `None` or `str`.
- `modnm` must be `None` or `str`.

Invalid payloads raise `ValueError`.

### Extracting a payload from a log message

Use `payload_of_log_msg()` when you only need the structured payload from a log message string.

```python
from ewts import payload_of_log_msg

payload = payload_of_log_msg(log_message)

if payload is not None:
    print(payload.status)
    print(payload.prog)
    print(payload.msg)
    print(payload.modnm)
```

`payload_of_log_msg()` returns `None` if no sentinel-wrapped payload is found. It raises `ValueError` if multiple payloads are found or if the sentinel-wrapped content cannot be parsed into a valid `Payload`.

### Parsing a full EWTS log line

Use `parts_of_log_line()` to parse a full standalone EWTS log line into `LogParts`.

```python
from ewts import parts_of_log_line

parts = parts_of_log_line(line)

print(parts.dt)       # datetime in UTC
print(parts.module)   # EWTS module id
print(parts.level)    # EWTS level name
print(parts.msg)      # message text
print(parts.payload)  # Payload or None
```

`parts_of_log_line()` expects a line with at least four whitespace-delimited parts:

```text
<timestamp> <module> <level> <message>
```

The timestamp must be an ISO timestamp with UTC timezone. If the line cannot be parsed, `parts_of_log_line()` raises `LogPartsFactoryParserError` unless `tolerant=True` is passed.

```python
parts = parts_of_log_line(line, tolerant=True)
```

In tolerant mode, non-payload parsing errors do not raise. Instead, fields that could not be parsed may be `None`. Payload parsing is still strict: if a payload sentinel is present, the payload must be valid.

## Recommended payload logging convention

Use EWTS `STATUS` level for high-level module status and progress messages. Use `Payload` when another process or post-run parser needs machine-readable status fields.

Example lifecycle:

```python
import ewts
from ewts import Payload, Status

LOG = ewts.get_logger(ewts.CAL_MGR_ID)

LOG.status(Payload(Status.INITTING, msg="Calibration manager initializing", modnm="cal-mgr"))
LOG.status(Payload(Status.INITTED, msg="Calibration manager initialized", modnm="cal-mgr"))
LOG.status(Payload(Status.STARTING, prog=0.0, msg="Calibration started", modnm="cal-mgr"))
LOG.status(Payload(Status.INPROG, prog=0.25, msg="Calibration 25% complete", modnm="cal-mgr"))
LOG.status(Payload(Status.COMPLETE, prog=1.0, msg="Calibration complete", modnm="cal-mgr"))
```

On failure:

```python
LOG.status(Payload(Status.ERROR, msg="Calibration failed", modnm="cal-mgr"))
LOG.severe("Calibration failed", exc_info=True)
```

## Module ids and keys

The package includes generated module registry constants. Module keys are lower-case, workflow-friendly identifiers. EWTS ids are the fixed-width logging identifiers used in log output.

| Module key | EWTS id | Description |
|---|---|---|
| `forcing` | `FORCING` | Forcing Engine |
| `lstm` | `LSTM` | Long Short-Term Memory Networks Model |
| `topoflow-glacier` | `TFGLACR` | Glacier Model from the TopoFlow Model |
| `t-route` | `TROUTE` | T-Route routing |
| `msw-mgr` | `MSWMGR` | Model Setup Workflow Component |
| `cal-mgr` | `CALMGR` | Calibration Manager Component |
| `eval-mgr` | `EVALMGR` | Evaluation Manager Component |
| `fcst-mgr` | `FCSTMGR` | Forecast Manager Component |
| `rte` | `RTE` | Runtime Environment Component |
| `assim-engine` | `ASSIM` | Assimilation Engine Component |

`get_logger()` and `setup_logger()` accept either a module key or an EWTS id. Unknown values are uppercased and used as the EWTS id.

```python
LOG1 = ewts.get_logger("t-route")   # resolves to TROUTE
LOG2 = ewts.get_logger("TROUTE")    # uses TROUTE
```

## Testing

From the Python package root:

```bash
cd runtime/python/ewts
python -m pytest
```

## Notes for maintainers

Generated files such as `log_levels.py`, `modules.py`, and `module_keys.py` are generated from EWTS specs and should not be edited directly. Update the source specs and regenerate language constants instead.
