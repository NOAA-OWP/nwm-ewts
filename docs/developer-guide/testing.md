# Testing Guide

EWTS testing should cover behavior rather than only build success. The important question is whether messages are routed, filtered, formatted, and interpreted correctly in each supported runtime mode.

## Test categories

| Category | Purpose |
| --- | --- |
| Unit tests | Validate constants, formatting, JSON construction, and filtering. |
| Runtime tests | Validate file/stdout fallback and bridge-mode selection. |
| Integration tests | Validate ngen/RTE-visible behavior. |
| Build tests | Validate install layout and package version. |
| Regression tests | Prevent known issues from returning. |

## Payload tests

| Payload | Expected result |
| --- | --- |
| Complete JSON payload | All fields preserved. |
| Missing `msg` | Empty message accepted. |
| Missing `prog` | Default progress applied where supported. |
| Invalid JSON | Error diagnostic/status emitted. |
| No wrapper | Treated as ordinary log text. |

## Demonstration guidance

When an error is difficult to inject in the full model, inspection is acceptable for unreachable or hard-to-trigger branches. Demonstrate representative logging behavior in RTE for normal progress and use code inspection to verify error paths emit EWTS messages when they are reached.
