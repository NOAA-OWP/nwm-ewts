# EWTS Installation Guide

------------------------------------------------------------------------

## Requirements

-   CMake ≥ 3.16
-   C/C++ compiler (GCC, Clang, Intel)
-   Fortran compiler (gfortran, ifort, ifx)
-   Python ≥ 3.11 (for Python runtime)

------------------------------------------------------------------------

## Build (Standalone Runtime)

cmake -B build -S runtime -DCMAKE_BUILD_TYPE=Release cmake --build build
-j

Install:

cmake --install build --prefix /path/to/install

------------------------------------------------------------------------

## Build with NGEN Integration

cmake -B build -S runtime -DCMAKE_BUILD_TYPE=Release -DEWTS_WITH_NGEN=ON
cmake --build build -j

------------------------------------------------------------------------

## Python Installation

Editable install:

pip install -e runtime/python/ewts

Standard install:

pip install runtime/python/ewts

Build wheel:

python -m build runtime/python/ewts

------------------------------------------------------------------------

## Troubleshooting

### Fortran line truncation warnings

Ensure free-form flags are enabled in CMake.

### Missing NGEN bridge symbols

Verify -DEWTS_WITH_NGEN=ON and bridge library availability.
