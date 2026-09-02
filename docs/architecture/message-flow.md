# Message Flow

## Standard log flow

```mermaid
sequenceDiagram
    participant M as Module code
    participant E as EWTS runtime
    participant B as ngen bridge
    participant O as Output
    M->>E: log(level, message) ewts id added in wrapper
    alt bridge enabled
        E->>B: ewts_ngen_log(ewts_id, level, message)
        B->>O: ngen log stream
    else standalone file configured
        E->>O: append formatted line to log file
    else stdout fallback
        E->>O: print formatted line to stdout
    end
```

## Payload log flow

```mermaid
sequenceDiagram
    participant M as Module code
    participant E as EWTS runtime
    participant B as ngen bridge
    participant R as RTE
    M->>E: status(<MSG_DATA>{json}</MSG_DATA>)
    E->>E: force logging regardless of min level
    E->>B: forward payload message
    B->>R: expose payload status/progress
```

## Filtering

| Level type | Filtered by minimum level? |
| --- | --- |
| DEBUG | Yes |
| PERFORM | Yes |
| INFO | Yes |
| WARNING | Yes |
| SEVERE | Yes |
| FATAL | Yes |
| STATUS | No |
