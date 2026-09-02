# Payload Status Messages

Payload status messages communicate operational state to RTE. They are emitted at the EWTS `STATUS` level and use structured JSON wrapped in `<MSG_DATA>` tags.

## Required wrapper

```text
<MSG_DATA>{"status":"IN_PROGRESS","prog":50.0,"msg":"Processing","modnm":"SAC_SMA"}</MSG_DATA>
```

## Payload fields

| Field | Type | Required | Default behavior | Description |
| --- | --- | --- | --- | --- |
| `status` | string | Recommended | Runtime may default to empty or ERROR for malformed payloads | Payload state. |
| `prog` | number | Optional | `0.0` when omitted in C++ extraction path | Progress value. |
| `msg` | string | Optional | Empty string is allowed | Human-readable status message. |
| `modnm` | string | Optional | Empty string when omitted | Module name for status attribution. |

## Status constants

| Status | JSON value | Meaning |
| --- | --- | --- |
| `NULL` | `NULL` | No specific state. |
| `INITTING` / `INITIALIZING` | `INITIALIZING` | Payload is initializing. |
| `INITTED` / `INITIALIZED` | `INITIALIZED` | Payload initialization completed. |
| `STARTING` | `STARTING` | Payload execution is starting. |
| `INPROG` / `IN_PROGRESS` | `IN_PROGRESS` | Payload work is in progress. |
| `COMPLETE` | `COMPLETE` | Payload completed successfully. |
| `ERROR` | `ERROR` | Payload encountered an error state. |

## Empty messages

An empty `msg` value is valid. A missing or empty message should not be treated as a malformed payload by itself.

## C++ extraction defaults

| Field | Default if omitted |
| --- | --- |
| `status` | Empty string |
| `prog` | `0.0` |
| `msg` | Empty string |
| `modnm` | Empty string |
