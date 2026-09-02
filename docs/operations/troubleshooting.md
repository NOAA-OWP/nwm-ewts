# Troubleshooting

## Python runtime appears stale

| Symptom | Check | Fix |
| --- | --- | --- |
| `bind_logger` or lazy-binding functions still exist | `grep -RniE "is_bound\|bind_logger\|get_bound_logger" <site-packages>/ewts` | Reinstall the wheel from `/opt/ewts/python/dist` with `--force-reinstall --no-cache-dir --no-deps`. |
| `pip show ewts` reports old version | `python -m pip show ewts` | Remove the old package and reinstall the intended wheel. |
| Import path points to unexpected location | `python -c "import ewts; print(ewts.__file__)"` | Fix virtual environment or image install order. |

## Payload logging problems

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| RTE does not see progress | Missing `<MSG_DATA>` wrapper or payload message emitted through the wrong path. | Emit the payload message using the documented payload convention and preserve the wrapper. |
| Payload parse error | Invalid JSON or unescaped string content. | Use JSON serialization helpers instead of manual concatenation. |
| Empty message treated as error | Incorrect validation rule. | Allow empty `msg`; only malformed JSON should be an error. |

## Fortran compilation problems

| Symptom | Cause | Fix |
| --- | --- | --- |
| Ambiguous log-level symbol | Imported and local constants both visible. | Rename imports or keep EWTS imports private. |
| Constant has no implicit type | Missing fallback or enabled definition. | Define the constant in both conditional branches. |
| Module ID has no implicit type | Name mismatch after copy/paste. | Use the local `MODULE_ID` consistently. |
