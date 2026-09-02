# Getting Started

## Choose the integration path

| Runtime context | Recommended integration |
| --- | --- |
| Python module code | Install ewts package and use `ewts.configure_existing_logger()`. |
| C module code | Include EWTS runtime C headers and call the C logging functions. |
| C++ module code | Include EWTS runtime C++ headers and call the C++ logging functions. |
| Fortran module code | Use the Fortran logger module wrapper and call the Fortran logging subroutines. |
| Running under ngen | Defaults to USE_EWTS=ON and modules use the ngen bridge methods. |
| Running standalone | Configure the EWTS_LOG_DIR environment variable or allow stdout fallback. |
| Python component code | Install ewts package and use `ewts.setup_logger()`. |

## Module Python example
```python
LOG = logging.getLogger("LSTM")
try:
    from ewts.helper import getenv_any
    from ewts.logger import configure_existing_logger
    LSTM_USE_EWTS = True
except ImportError:
    LSTM_USE_EWTS = False

...

def __init__(self) -> None:
    if LSTM_USE_EWTS:
        # Determine if running within ngen using EWTS. This must be done  
        # here when the model actually runs vs when it is imported 
        # into the ngen Python interpreter to ensure the env vars are set.
        val = getenv_any("EWTS_USE_NGEN_BRIDGE", "").strip().lower()
        if val in {"1", "true", "yes", "on"}:
            configure_existing_logger(LOG)
        else:
            _configure_stdout_logging()
            LOG.warning("ewts package installed but EWTS_USE_NGEN_BRIDGE not on. Falling back to default logging.")
    else:
        _configure_stdout_logging()
```

## Non-ngen module Python example
```python
from ewts import setup_logger

LOG = setup_logger(
    ewts_id="MY_COMPONENT",
    level="INFO",
    log_dir="./logs",
    log_file_name="my_component.log",
    running_in_ngen=False,
)

LOG.info("Starting component")
LOG.status('<MSG_DATA>{"status":"STARTING","prog":0.0,"msg":"Starting model","modnm":"MY_COMPONENT"}</MSG_DATA>')
```

## Runtime decision checklist

| Question | Why it matters |
| --- | --- |
| Is the code running inside ngen? | Determines whether messages should go through the bridge. |
| What is the EWTS ID? | Controls message attribution and module split behavior. |
| Should logs go to a file? | If not, standalone mode falls back to stdout. |
| Are payload messages required? | RTE progress visibility depends on payload messages. |
| Is a pre-existing Python logger already created? | Use `configure_existing_logger()` instead of replacing it. |
