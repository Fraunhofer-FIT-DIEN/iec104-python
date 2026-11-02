# iec104-python

High-level Python module for simulating SCADA systems and RTUs communicating via the IEC 60870-5-104 protocol. Built with lib60870-C and pybind11.

## 🚀 Quick Start

### Installation
```bash
pip install c104
```

### Basic Usage
**SCADA Unit (Client):**
```python
import c104
client = c104.Client()
connection = client.add_connection(ip="127.0.0.1", port=2404, init=c104.Init.INTERROGATION)
station = connection.add_station(common_address=47)
point = station.add_point(io_address=11, type=c104.Type.M_ME_NC_1)
client.start()
```

**Remote Terminal Unit (Server):**
```python
import c104
server = c104.Server(ip="0.0.0.0", port=2404)
station = server.add_station(common_address=47)
station.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=5000)
server.start()
```
See the [examples](https://github.com/Fraunhofer-FIT-DIEN/iec104-python/tree/main/examples) folder for more detailed scenarios.

## 🛠 Installation & Build

### Standard Install
Install via PyPI: `pip install c104` or from Git: `pip install c104@git+https://github.com/fraunhofer-fit-dien/iec104-python.git`

### Building from Source
**System Requirements**: Python $\ge 3.9, <= 3.15$.

1. **Clone & Init**:
   ```bash
   git clone --depth=1 --branch=main https://github.com/Fraunhofer-FIT-DIEN/iec104-python.git
   cd iec104-python
   git submodule update --init
   ```

2. **Build Wheel**:
   - **Linux**: `python3 -m pip wheel .`
   - **Windows**: `python3 -m pip wheel .` (requires VS 2022 Build Tools)
   - **Multi-version (Linux Docker)**: `/bin/bash ./bin/linux-build.sh`

3. **Custom Windows Build (MSVC)**:
   ```powershell
   cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -B cmake-build-release -A x64 -DPython_EXECUTABLE=C:\PATH_TO_PYTHON\python.exe
   &"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" /m /p:Platform=x64 /p:Configuration=Release c104.sln /t:Rebuild
   ```
   Set a valid PATH_TO_PYTHON, if you have multiple python versions. \
   Set a valid path to MSBuild.exe unless msbuild is already in path.

## 📚 Documentation & Contribution
- **API Docs**: See our [ReadTheDocs](https://iec104-python.readthedocs.io/latest/index.html) for detailed class and property references.
- **Contributing**: Report bugs or submit feature requests via GitHub Issues.
- **Build Docs**: `python3 bin/build-docs.py` (requires Doxygen & Graphviz).

## ⚖️ Licensing & Dependencies
Licensed under **GPLv3**.

| Dependency | License | Purpose |
|---|---|---|
| [lib60870-C](https://github.com/mz-automation/lib60870) | GPLv3 | Core protocol implementation |
| [mbedtls](https://github.com/Mbed-TLS/mbedtls) | Apache-2.0 | Transport Layer Security (TLS) |
| [pybind11](https://github.com/pybind/pybind11) | BSD | C++ $\to$ Python bindings |
| [catch2](https://github.com/catchorg/Catch2) | BSL-1.0 | C++ Unit Testing |
