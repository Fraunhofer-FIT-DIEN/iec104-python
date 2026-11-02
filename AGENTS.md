# AGENTS.md — iec104-python

## 🚨 CRITICAL: AVOID THESE MISTAKES
- **Public API Source**: The `.pyi` file is a mirror; the **C++ `src/python.cpp` is the source of truth**. Add new public API in C++, then update the `.pyi` stub.
- **Type Stubs**: Any change to a C++ binding signature **MUST** be mirrored in `c104/__init__.pyi`.
- **Build System**: This is CMake + setuptools. Submodules (`depends/`) are **mandatory**; run `git submodule update --init` first.
- **Source Registration**: New C++ files must be added to `c104_SOURCES` in `CMakeLists.txt` for both the module and the `c104_tests` target.
- **Package Name**: The pip package is `c104`, while the repo is `iec104-python`.

## Project Structure & Architecture
The `c104` package wraps `lib60870-C` (+ mbedtls) via pybind11.

| Path | Purpose |
|------|----------|
| `c104/` | Python package: re-exports `_c104` C++ extension and configures logging. |
| `src/` | C++ Bindings: `python.cpp` (entry), `Client`, `Server`, `object/` (Points/Stations), `remote/` (Connections). |
| `depends/` | Git submodules: `lib60870`, `mbedtls`, `pybind11`, `catch`. |
| `tests/` | Verification: Python scripts and C++ Catch2 tests. |
| `bin/` | Build & tool scripts (Linux/Windows/Docs). |
| `docs/` | Documentation source (Sphinx/Breathe/Doxygen). |

**Logic Flow**: `Client` $\to$ `Connection` $\to$ `Station` $\to$ `Point`. `Server` $\to$ `Station`.
**GIL**: Handled via `ScopedGilAcquire`/`ScopedGilRelease` in `src/module/`.

## Build & Verification

### Build Commands
- **Submodules**: `git submodule update --init`
- **Linux/Windows (Wheel)**: `python3 -m pip wheel .`
- **Windows (MSVC/Pyd)**: Use `cmake -G "Visual Studio 17 2022" -B cmake-build-release ...`
- **Python Version**: $\ge 3.9, <= 3.15$ (Free-threaded 3.13t supported).

### Testing
- **C++ Tests**: Built as `c104_tests` (Catch2). Run via `ctest` or `./c104_tests`.
- **Python Tests**: `python tests/test.py` or `test_variant_values.py`.
- **Integration**: `server.py` $\to$ `client.py` $\to$ `loop.py` smoke tests.

## Standards
- **Python**: Black (`line-length=120`), `py39-py315`.
- **C++**: `CMAKE_CXX_STANDARD 17`, clang-format.
- **CI**: GitHub Actions $\to$ PyPI (via trusted publishing).
- **License**: GPLv3.
