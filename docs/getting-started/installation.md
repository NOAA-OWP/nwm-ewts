# Installation and Build

EWTS is usually installed as part of a container image build. The installed artifacts include native libraries, headers, the ngen bridge, and optionally the Python package.

## Typical install prefix

| Artifact | Common location |
| --- | --- |
| Headers | `/opt/ewts/include` |
| Libraries | `/opt/ewts/lib` or `/opt/ewts/lib64` |
| Python wheel | `/opt/ewts/python/dist/ewts-*.whl` |
| Git provenance | Image-specific JSON such as `/ngen-app/nwm-ewts_git_info.json` |

## CMake configuration

```bash
cmake -S . -B cmake_build \
  -DEWTS_WITH_NGEN=ON \
  -DEWTS_BUILD_SHARED=ON
cmake --build cmake_build --parallel
cmake --install cmake_build --prefix /opt/ewts
```

## Python package installation

```bash
python -m pip uninstall -y ewts || true
python -m pip install --force-reinstall --no-cache-dir --no-deps /opt/ewts/python/dist/ewts-*.whl
```

## Runtime library path

```bash
export LD_LIBRARY_PATH="/opt/ewts/lib:/opt/ewts/lib64:${LD_LIBRARY_PATH}"
```

## Validation checks

| Check | Command |
| --- | --- |
| Python package location | `python -c "import ewts; print(ewts.__file__)"` |
| Python logger location | `python -c "import ewts.logger as l; print(l.__file__)"` |
| Python version metadata | `python -m pip show ewts` |
| Stale lazy-binding symbols | `grep -RniE "is_bound\|bind_logger\|get_bound_logger" <site-packages>/ewts` |

The Python commands must be run from the installed Python environment, which could be locally, the Docker container or IaC container.
