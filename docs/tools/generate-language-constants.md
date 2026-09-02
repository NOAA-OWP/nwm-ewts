# Language Constants Generator

The **Language Constants Generator** creates the language-specific
constants, lookup tables, helper APIs, log-level definitions, and
payload status definitions used by the **Error and Warning Trapping
System (EWTS)**. Rather than maintaining these files by hand, the
generator produces them directly from the repository specification
files, ensuring every supported language remains synchronized.

## Purpose

The generator provides a single source of truth for:

-   Module identifiers (EWTS IDs)
-   Module keys and descriptions
-   Module lookup helper APIs
-   Log level definitions
-   Payload status definitions

Generated outputs are produced for:

-   C
-   C++
-   Fortran
-   Python
-   the `ngen` integration layer

This eliminates duplicated definitions, prevents drift between language
implementations, and guarantees consistent runtime behavior.

## Input Specifications

The generator reads the following specification files:

  ------------------------------------------------------------------------
  File                          Purpose
  ----------------------------- ------------------------------------------
  `spec/module_registry.yaml`   Registered modules, EWTS IDs, languages,
                                and descriptions

  `spec/log_levels.json`        Standard EWTS log levels

  `spec/payload_status.json`    Payload status string definitions
  ------------------------------------------------------------------------

These files are the only files that should be edited manually.

## Generated Outputs

Depending on the selected generation target, the tool creates:

### Runtime

-   Module constants
-   Module lookup helpers
-   Log level definitions
-   Payload status definitions

for the C, C++, Fortran, and Python runtimes.

### `ngen` Integration

The `ngen` integration receives aggregate generated headers containing
every registered module regardless of implementation language, allowing
the bridge to translate module keys into EWTS IDs consistently.

## Running the Generator

Generate everything:

``` bash
python tools/generate_language_constants.py
```

Generate only module-related files:

``` bash
python tools/generate_language_constants.py --generate modules
```

Generate only log-level definitions:

``` bash
python tools/generate_language_constants.py --generate log-levels
```

Generate only payload status definitions:

``` bash
python tools/generate_language_constants.py --generate payload-status
```

Specify an alternate repository root:

``` bash
python tools/generate_language_constants.py \
    --repo-root /path/to/repository
```

Specify alternate specification files:

``` bash
python tools/generate_language_constants.py \
    --registry spec/module_registry.yaml \
    --log-levels spec/log_levels.json \
    --payload-status spec/payload_status.json
```

## Validation

Before generating files, the tool validates the specifications.

Examples include:

-   Required fields are present.
-   Module keys are unique.
-   EWTS IDs are uppercase and no more than eight characters.
-   Languages are limited to C, C++, Fortran, and Python.
-   Log levels contain valid integer values.
-   Payload status definitions contain valid strings.

Generation stops immediately if any validation error is detected.

## Generated Files

Generated files include:

-   Language-specific module constants
-   Module lookup helpers
-   Runtime log level definitions
-   Runtime payload status definitions
-   Aggregate `ngen` integration headers

Each generated file contains a banner documenting:

-   Generation timestamp
-   Source specification
-   Specification version
-   Specification generation timestamp (when available)

Generated files should **never** be edited manually. Modify the
specification files and rerun the generator instead.

## Developer Notes

The generator is intended for repository maintainers. Runtime users
should use the generated constants rather than the generator itself.

The generator is idempotent and may be safely rerun whenever the
specification files change.
