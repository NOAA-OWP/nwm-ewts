# Configuration

EWTS configuration controls message filtering, output destination, and bridge behavior.

### Environment variables

| Variable | Set By | Purpose |
| --- | --- | --- |
| `NGEN_RESULTS_DIR` | Caller of ngen | Directory for ngen log files and the starting point for locating the logging configuration file. |
| `EWTS_USE_NGEN_BRIDGE` | ngen | Signals runtime code to route through the ngen bridge when available. |
| `EWTS_ENABLED` | ngen | EWTS logging enabled or disabled. Based on the logging configuration file. |
| `<module>_LOGLEVEL` | ngen | Individual module log level. Based on the logging configuration file. |
| `RUN_LOGS_DIR` | RTE | Common RTE-provided log directory for run output used by the components. |
| `LD_LIBRARY_PATH` | Deployment environment | Must include EWTS library directories for native bridge loading. |
| `EWTS_PREFIX` | CMakeLists.txt | Root install prefix, commonly `/opt/ewts`. |

# Common fields

| Field | Meaning | Typical value |
| --- | --- | --- |
| `ewts_id` | Component or module identifier | `SAC_SMA`, `SNOW17`, `TROUTE` |
| `level` | Minimum diagnostic level | `INFO`, `WARNING`, or numeric level |
| `log_dir` | Directory for standalone log output | RTE run log directory or local `./logs` |
| `log_file_name` | File name for standalone output | `model.log` |
| `running_in_ngen` | Whether to use ngen bridge routing | `true` inside ngen/RTE |
| `enabled` | Whether EWTS logging is enabled | Build/runtime dependent |


## Python logger adoption

| Use `setup_logger()` when... | Use `configure_existing_logger()` when... |
| --- | --- |
| EWTS should create the component logger. | Application code already created the logger. |
| New component is being integrated. | A third-party or existing module logger identity must be preserved. |
