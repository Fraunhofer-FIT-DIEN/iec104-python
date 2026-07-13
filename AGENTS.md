# AGENTS.md — iec104-python

## What this is

Python C++ pybind11 wrapper for IEC 60870-5-104 SCADA/RTU protocol. The `c104` pip package wraps `lib60870-C` (+ mbedtls for TLS) via CMake + pybind11.

## Project structure

| Path | What |
|------|------|
| `c104/` | Python package (re-exports `_c104` C++ extension, sets up logging) |
| `src/` | C++ pybind11 bindings (`python.cpp` is the module entry; `Client.h/.cpp`, `Server.h/.cpp`, `object/`, `remote/`) |
| `depends/` | Git submodules: `lib60870`, `mbedtls`, `pybind11`, `catch` |
| `tests/` | Python scripts + C++ Catch2 tests (`main.cpp` + `test_*.cpp`) |
| `bin/` | Build scripts for Linux (manylinux Docker), Windows, docs |
| `docs/` | Sphinx + Breathe + Doxygen docs, hosted on readthedocs |

## Build (must know)

```bash
# Required: submodules first
git submodule update --init
```

**Linux:** `python3 -m pip wheel .`

**Windows (Powershell, VS 2022):**
```powershell
cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -B cmake-build-release -A x64 -DPython_EXECUTABLE=C:\PATH_TO_PYTHON\python.exe
&"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" /m /p:Platform=x64 /p:Configuration=Release c104.sln /t:Rebuild
```

**Windows (wheel, any VS version):** `python3 -m pip wheel .`

**Multi-version via Docker (linux):** `/bin/bash ./bin/linux-build.sh`

**Build dependencies in `pyproject.toml`:** `setuptools`, `wheel`, `ninja`, `cmake>=3.18,<3.28` (3.28+ requires libssl.3 — pinned intentionally).

**Python:** >= 3.7, < 3.13 (free-threaded 3.13t supported — CMake detects `Py_GIL_DISABLED`).

## Architecture

- **Single C++ extension module** named `_c104` built via CMake + pybind11.
- `c104/__init__.py` re-exports everything from `_c104` into the `c104` namespace and sets up logging (`logging.getLogger("c104")`, `INFO` level, StreamHandler).
- Two main classes: `c104.Client` (SCADA/master) and `c104.Server` (RTU/slave).
- `c104.Client` owns `Connection` objects; each `Connection` owns `Station` objects; each `Station` owns `Point` objects.
- `c104.Server` owns `Station` objects directly.
- GIL management: `ScopedGilAcquire`/`ScopedGilRelease`/`GilAwareMutex` in `src/module/`.
- TLS: `TransportSecurity` class, supports version selection and cipher selection.
- CMake conditionally copies mbedtls into lib60870's dependency tree for TLS builds.

## Testing

**Catch2 C++ tests** (built via CMake when `depends/catch` exists):
- `tests/main.cpp` + `test_object_datapoint.cpp`, `test_object_station.cpp`, `test_remote_message_batch.cpp`, `test_server_interrogation.cpp`
- Built as `c104_tests` executable; registered via CTest.

**Python test scripts** in `tests/`:
- `test.py`, `test_variant_values.py` — run as `python tests/test.py`
- `server.py`, `client.py`, `loop.py` — integration smoke tests (start server, then client)
- `server_passive.py`, `client_reconnect.py`, `transmit.py`
- TLS tests use certs from `tests/certs/`

To run C++ tests (from build dir): `ctest` or `./c104_tests`.

## Code style

- **Python:** Black with `line-length=120`, `skip-string-normalization=true`, target `py37-py313`.
- **C++:** `CMAKE_CXX_STANDARD 17`, clang-format via pre-commit.
- **Pre-commit:** `pre-commit install` — checks toml/yaml, trailing whitespace, clang-format, shellcheck, cmake-format.
- No linting/typechecking of Python code in CI beyond pre-commit.

## CI / Release

- **CI:** GitHub Actions on every push. Builds wheel + sdist via `python3 -m build`.
- **PyPI publish:** Only on tag pushes (`refs/tags/`). Uses trusted publishing (id-token). Package name: `c104`.
- **ReadTheDocs:** Configured via `.readthedocs.yml` (clones submodules, Python 3.10, Sphinx conf at `docs/source/conf.py`).

## Key things an agent will get wrong

- **The Python package `__init__.py` is NOT the main source of public API.** Most classes (`Client`, `Server`, `Point`, `Station`, `Connection`, types, enums) are defined in C++ `python.cpp` and re-exported via `vars(_c104)`. Add new public API in C++, not Python.
- **Type stubs** live in `c104/__init__.pyi` — if you change a C++ binding's signature, update the `.pyi` file.
- **Build system is CMake + setuptools**, not pure Python. `setup.py` has a custom `CMakeBuild` command.
- **Submodules are required.** Missing them = build fails. `MANIFEST.in` includes all dependency sources for sdist.
- **`c104` is the pip package name**, not `iec104-python`. The repo name differs from the package name.
- **New C++ source files** must be added to `c104_SOURCES` in `CMakeLists.txt` (both for `_c104` module AND for the `c104_tests` test executable).
- **Logging is configured at import time** — `c104/__init__.py` adds a StreamHandler. Buffered stdout (`PYTHONUNBUFFERED=1` required) is warned on import.
- **GPLv3 licensed** — dependencies carry their own licenses (Apache-2.0 mbedtls, BSD pybind11, BSL-1.0 Catch2).
