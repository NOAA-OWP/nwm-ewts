# Architecture Overview

EWTS is a layered logging system. Module code uses language-specific APIs and send their messages to ngen through the ewts ngen bridge methods. The ngen logger normalize levels, module IDs, and payload status conventions. At runtime, messages to the ngen bridge. If the bridge is unavailable, logs are written to standalone output.

## NGEN log usage and message flow

```mermaid
flowchart TB
    subgraph Module_Code[Modules]
        PyCode[Python]
        CCode[C]
        CppCode[C++]
        FCode[Fortran]
        Wrappers[Wrappers]
    end
    subgraph Wrappers[Wrappers]
        FWrapper[use ewts logger]
        PtyhonBmi[import ewts package]
        CHeader[include ewts header]
        CPPHeader[include ewts header]
    end
    subgraph EWTS_Runtime[EWTS runtime layer]
        PyRuntime[Python package]
        CRuntime[C runtime]
        CPPRuntime[CPP runtime]
        FRuntime[Fortran runtime]
    end
    subgraph EWTS_Integration[EWTS Integration layer]
        Bridge[ewts_ngen_bridge]
        NgenLogger[ngen logger]
    end
    subgraph Outputs[Outputs]
        OutputRTE[Consumed by RTE]
        OutputStandalone[Standalone]
    end
    subgraph OutputRTE[Consumed by RTE]
        StandardLogs[Standard logs]
        PayloadLogs[Payload logs]
        Stdout[stdout]
    end
    subgraph OutputStandalone[Standalone]
        SAStandardLogs[Standard logs]
        SAPayloadLogs[Payload logs]
        SAStdout[stdout]
    end
    PyCode --> PtyhonBmi
    CCode --> CHeader
    CppCode --> CPPHeader
    FCode --> FWrapper
    FWrapper --> FRuntime
    PtyhonBmi --> PyRuntime
    CHeader --> CRuntime
    CPPHeader --> CPPRuntime
    FRuntime --> Bridge
    PyRuntime --> Bridge
    CRuntime --> Bridge
    CPPRuntime --> Bridge
    Bridge --> NgenLogger
    NgenLogger --> StandardLogs
    NgenLogger --> PayloadLogs
    NgenLogger --> Stdout
```

## Layer responsibilities

| Layer | Responsibilities |
| --- | --- |
| Modules | Calls the local EWTS API with module ID, level, and message. |
| Language runtime | Selects bridge or standalone output. |
| ngen bridge | Forwards messages into ngen logger to handle formatting and file I/O. |
| Output layers | Stores standard, payload and stdout logs for operational consumers. |
