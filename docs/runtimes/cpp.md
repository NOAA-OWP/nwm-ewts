# C++ Runtime Guide

The C++ runtime library is the module-facing EWTS interface for modules written in C++ that supports standard EWTS levels, Payload messages, file/stdout fallback, and ngen bridge routing. It is documented separately from the ngen logger and bridge integration.

## Intended use

| Use case | Description |
| --- | --- |
| C++ module logging | Allow C++ modules to emit EWTS diagnostic messages with a module-specific EWTS ID. |
| Consistent EWTS levels | Use the same level semantics as the Python, C, and Fortran runtimes. |
| Payload logging | Allow modules to emit structured payload messages using the documented EWTS payload convention. |
| ngen execution | Route module messages through the EWTS integration path when the module is executed under ngen. |
| Standalone execution | Support module execution outside ngen where logging falls back to configured file output or stdout. |

## Relationship to the ngen bridge

| Item | Role |
| --- | --- |
| C++ runtime library | Module-facing API used by C++ model code. |
| EWTS ngen bridge | Integration layer that forwards EWTS messages into the ngen logging path. |
| ngen logger | ngen-owned logging infrastructure. |

```mermaid
flowchart LR
    Module["C++ model module"] --> Runtime["EWTS C++ runtime"]
    Runtime --> Bridge{"Running under ngen?"}
    Bridge -- Yes --> NgenBridge["EWTS ngen bridge"]
    NgenBridge --> Ngen["ngen logging path"]
    Bridge -- No --> Standalone["Standalone file or stdout output"]
```

## Runtime responsibilities

| Responsibility | Description |
| --- | --- |
| Level mapping | Preserve EWTS log-level semantics for C++ module code. |
| Module identity | Include the EWTS ID so messages can be attributed to the correct module. |
| Message forwarding | Pass module messages into the EWTS native logging layer. |
| Payload message support | Allow payload messages to be emitted using the documented `<MSG_DATA>` wrapper and JSON fields. |
| Standalone fallback | Preserve usable output when the ngen bridge is not active. |

## Payload message guidance

C++ modules that report progress or execution state should use the same payload message convention as the other runtimes.

| Field | Default or expectation |
| --- | --- |
| `status` | Payload execution state such as `INITIALIZING`, `IN_PROGRESS`, `COMPLETE`, or `ERROR`. |
| `prog` | Progress value when available. |
| `msg` | Optional human-readable status message. Empty values are allowed. |
| `modnm` | Module name or EWTS ID used by downstream consumers. |

Use a JSON serialization helper rather than manually concatenating JSON field values whenever practical. This prevents malformed payload messages caused by unescaped quotes, embedded newlines, or other special characters.

