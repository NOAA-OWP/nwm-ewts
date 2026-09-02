# EWTS Language Constants Generator

This directory contains the developer-facing documentation for
`tools/generate_language_constants.py`.

The generator creates language-specific module constants, module-key helpers,
and log-level definitions from repository specification files.

## Purpose

The script keeps constants synchronized across:

- C runtime outputs
- C++ runtime outputs
- Fortran runtime outputs
- Python runtime outputs
- aggregate `ngen` integration headers

This preserves the specification files as the single source of truth.

## Inputs

The generator reads:

- `spec/module_registry.yaml`
- `spec/log_levels.json`

### Registry rules

The module registry must define a top-level `modules` list. Each module entry
must include:

- `key`
- `ewts_id`
- `language`

Optional fields such as `description` may also be present.

The generator enforces the following rules:

- `modules` must exist and be a list
- `key` must be non-empty
- `ewts_id` must be non-empty
- `language` must be one of `c`, `cpp`, `fortran`, or `python`
- `ewts_id` must be uppercase
- `ewts_id` must be at most 8 characters long
- module `key` values must be unique
- duplicate `ewts_id` values are allowed

Duplicate `ewts_id` values can be useful for aliases.

Registry YAML file snippet
```
version: 1
modules:
  - key: ngen
    ewts_id: NGEN
    language: cpp
    description: "ngen framework"
  - key: forcing
    ewts_id: FORCING
    language: python
    description: "Forcing Engine"
  - key: lasam
    ewts_id: LASAM
    language: cpp
    description: "Lumped Arid/Semi-arid Model"
  - key: lstm
    ewts_id: LSTM
    language: python
    description: "Long Short-Term Memory Networks Model"
  - key: noah-owp-modular
    ewts_id: NOAHOWP
    language: fortran
    description: "Noah OWP Land Surface Model"
  - key: pet
    ewts_id: PET
    language: c
    description: "Potential Evapotranspiration Model"
```

## Outputs

### Runtime outputs

The generator filters runtime outputs by the `language` field in the module
registry.

Examples include:

- `runtime/c/include/ewts/module_keys.h`
- `runtime/c/include/ewts/module_constants.h`
- `runtime/cpp/include/ewts/module_keys.hpp`
- `runtime/cpp/include/ewts/module_constants.hpp`
- `runtime/fortran/src/ewts/module_constants.f90`
- `runtime/python/ewts/src/ewts/module_keys.py`
- `runtime/python/ewts/src/ewts/log_levels.py`

### `ngen` integration outputs

The integration layer receives aggregate generated headers spanning all modules,
for example:

- `integrations/ngen/include/ewts_ngen/ngen_module_constants.hpp`
- `integrations/ngen/include/ewts_ngen/ngen_module_keys.hpp`

## Run the generator

Default usage:

```bash
python tools/generate_language_constants.py
```

Explicit input paths:

```bash
python tools/generate_language_constants.py   --registry spec/module_registry.yaml   --log-levels spec/log_levels.json
```

Explicit repository root:

```bash
python tools/generate_language_constants.py   --repo-root /path/to/repo   --registry spec/module_registry.yaml   --log-levels spec/log_levels.json
```

## Developer guidance

Generated files should be treated as derived artifacts and should not be edited
manually. Update the specification files instead, then regenerate outputs.

## Related documentation

- user-facing overview: `docs/tools/generate-language-constants.md`
- generated constants architecture: `docs/architecture/generated-constants.md`
