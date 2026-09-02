# Validation Checklist

## Build validation

| Check | Pass criteria |
| --- | --- |
| EWTS source ref recorded | Image contains branch/commit metadata. |
| Native libraries installed | EWTS libraries exist under `/opt/ewts/lib` or `/opt/ewts/lib64`. |
| Headers installed | EWTS headers exist under `/opt/ewts/include`. |
| Python wheel installed when needed | `python -m pip show ewts` reports intended package. |
| ngen bridge built | Bridge library exists and loads. |

## Runtime validation

| Check | Pass criteria |
| --- | --- |
| Diagnostic INFO message appears | INFO log is visible at configured level. |
| DEBUG filtering works | DEBUG suppressed when minimum level is INFO. |
| STATUS bypasses filter | STATUS message appears even if minimum level is higher. |
| Multiline messages are readable | Each line is formatted consistently. |
| Standalone fallback works | Logs appear in file or stdout outside ngen. |

## Regression validation

| Area | Regression to prevent |
| --- | --- |
| Python runtime | Reintroduction of lazy binding. |
| Fortran wrappers | Symbol ambiguity from public imports. |
| C++ payload extraction | Empty `msg` incorrectly rejected. |
| Docker images | Mixed EWTS refs across image layers. |
