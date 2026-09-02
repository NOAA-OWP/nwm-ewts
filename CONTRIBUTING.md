# Contributing to EWTS

Thank you for contributing to EWTS.

## Development Setup

1.  Clone repository
2.  Create virtual environment for Python
3.  Install Python runtime in editable mode: pip install -e
    runtime/python/ewts

## Build & Test

cmake -B build -S runtime -DCMAKE_BUILD_TYPE=Release cmake --build build
-j

Run Python tests (if applicable):

pytest -q

## Coding Standards

-   Maintain cross-language API consistency
-   Preserve thread safety in C and C++ runtimes
-   Ensure Fortran remains free-form compatible
-   Follow semantic versioning for releases

## Pull Requests

-   Provide clear description
-   Reference related issues
-   Ensure builds pass with EWTS_WITH_NGEN=ON and OFF
