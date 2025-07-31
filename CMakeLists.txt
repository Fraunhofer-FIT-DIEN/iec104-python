cmake_minimum_required(VERSION 3.15)
project(c104 LANGUAGES CXX)

# ##############################################################################
# COMPILE FLAGS
# ##############################################################################

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
  # MSVC supports /W0 to /W4 or /Wall
  add_compile_options(/W3)
else()
  # GCC or Clang
  add_compile_options(-Wno-attributes)
endif()

message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "ModulePath: ${CMAKE_MODULE_PATH}")

if(DEFINED ENV{TOOLCHAIN})
  message(STATUS "TOOLCHAIN: $ENV{TOOLCHAIN}")
endif()

list(APPEND c104_PRIVATE_DEFINITIONS VERSION_INFO=\"${C104_VERSION_INFO}\")

# ##############################################################################
# pybind11 and Python3
# ##############################################################################

message(STATUS "Add Pybind11")
set(Python_FIND_STRATEGY "LOCATION")
set(Python_FIND_REGISTRY "LAST")
set(PYBIND11_FINDPYTHON ON)
add_subdirectory(depends/pybind11)
list(APPEND c104_tests_PRIVATE_LIBRARIES pybind11::embed)
if(WIN32)
  list(APPEND c104_tests_PRIVATE_LIBRARIES pybind11::windows_extras)
endif()

# Run Python to check for GIL status
execute_process(
  COMMAND
    ${Python_EXECUTABLE} -c
    "import sysconfig; print(bool(sysconfig.get_config_var('Py_GIL_DISABLED')) and 1 or 0)"
  OUTPUT_VARIABLE PYTHON_FREE_THREADED
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(PYTHON_FREE_THREADED)
  # set Py_GIL_DISABLED=1 to build pybind11 for free threaded python
  list(APPEND c104_PRIVATE_DEFINITIONS Py_GIL_DISABLED=1)
  message(STATUS "Python is running without GIL (free threaded)")

  # fix library path on windows
  if(WIN32 AND DEFINED Python_LIBRARY)
    get_filename_component(_PYTHON_LIB_DIR "${Python_LIBRARY}" DIRECTORY)
    get_filename_component(_PYTHON_LIB_NAME "${Python_LIBRARY}" NAME_WE)
    get_filename_component(_PYTHON_LIB_EXT "${Python_LIBRARY}" EXT)

    # Check if library name already has 't' suffix
    if(NOT _PYTHON_LIB_NAME MATCHES "t$")
      set(_PYTHON_LIB_NAME "${_PYTHON_LIB_NAME}t")
      set(_CORRECTED_PYTHON_LIBRARY
          "${_PYTHON_LIB_DIR}/${_PYTHON_LIB_NAME}${_PYTHON_LIB_EXT}")

      message(STATUS "Hotfix free-threaded Python library path:")
      message(STATUS "  Original: ${Python_LIBRARY}")
      message(STATUS "  Corrected: ${_CORRECTED_PYTHON_LIBRARY}")

      # Verify the corrected library exists
      if(EXISTS "${_CORRECTED_PYTHON_LIBRARY}")
        set(Python_LIBRARY
            "${_CORRECTED_PYTHON_LIBRARY}"
            CACHE STRING "Path to Python library" FORCE)
        set(Python_LIBRARIES
            "${_CORRECTED_PYTHON_LIBRARY}"
            CACHE STRING "Python libraries" FORCE)
      else()
        message(
          WARNING
            "Corrected Python library file not found: ${_CORRECTED_PYTHON_LIBRARY}"
        )
      endif()
    endif()

  endif()
else()
  message(STATUS "Python is running with GIL (classic)")
endif()

# ##############################################################################
# atomic
# ##############################################################################

set(ATOMIC_TEST_SOURCE
    "
        #include <atomic>
        int main() { std::atomic<int64_t> i(0); i++; return 0; }
    ")
check_cxx_source_compiles("${ATOMIC_TEST_SOURCE}" ATOMIC_INT64_IS_BUILTIN)
if(NOT ATOMIC_INT64_IS_BUILTIN)
  set(CMAKE_REQUIRED_LIBRARIES atomic)
  check_cxx_source_compiles("${ATOMIC_TEST_SOURCE}"
                            ATOMIC_INT64_REQUIRES_LIBATOMIC)
  if(ATOMIC_INT64_REQUIRES_LIBATOMIC)
    list(APPEND c104_PRIVATE_LIBRARIES atomic)
    list(APPEND c104_tests_PRIVATE_LIBRARIES atomic)
  endif()
  unset(CMAKE_REQUIRED_LIBRARIES)
endif()

# ##############################################################################
# lib60870-C 2.3.3
# ##############################################################################
message(STATUS "Add lib60870")
# Change the .lib suffix for static libs to avoid conflict with DLL import .lib
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a") # or ".lib_static"
set(BUILD_COMMON
    ON
    CACHE BOOL "Build the platform abstraction layer (HAL)")
set(BUILD_HAL
    ON
    CACHE BOOL
          "Build common code (shared with other libraries - e.g. libiec61850)")
set(BUILD_EXAMPLES
    OFF
    CACHE BOOL "Build the examples")
set(BUILD_TESTS
    OFF
    CACHE BOOL "Build the tests")
if(EXISTS "${PROJECT_SOURCE_DIR}/depends/mbedtls/library")
  message(STATUS "> copy mbedtls")
  file(
    MAKE_DIRECTORY
    "${PROJECT_SOURCE_DIR}/depends/lib60870/lib60870-C/dependencies/mbedtls-3.6"
  )
  file(
    COPY depends/mbedtls/library depends/mbedtls/include
         depends/mbedtls/CMakeLists.txt
    DESTINATION
      "${PROJECT_SOURCE_DIR}/depends/lib60870/lib60870-C/dependencies/mbedtls-3.6"
  )
else()
  if(EXISTS
     "${PROJECT_SOURCE_DIR}/depends/lib60870/lib60870-C/dependencies/mbedtls-3.6/library"
  )
    message(STATUS " > Use existing copy of mbedtls")
  else()
    message(FATAL_ERROR "> mbedtls not found")
  endif()
endif()
add_subdirectory(depends/lib60870/lib60870-C)
list(APPEND c104_PRIVATE_LIBRARIES lib60870)
list(APPEND c104_tests_PRIVATE_LIBRARIES lib60870)

# ##############################################################################
# c104
# ##############################################################################

include_directories(
  src depends/lib60870/lib60870-C/src/inc/api
  depends/lib60870/lib60870-C/src/hal/inc
  depends/lib60870/lib60870-C/src/common/inc depends/mbedtls/include)

set(c104_SOURCES
    src/bitflag.h
    src/debug.h
    src/numbers.h
    src/enums.h
    src/types.h
    src/module/Tuple.h
    src/module/Callback.h
    src/module/ScopedGilAcquire.h
    src/module/ScopedGilRelease.h
    src/module/GilAwareMutex.h
    src/object/DateTime.h
    src/object/information/IInformation.h
    src/object/information/Generic.h
    src/object/information/ICommand.h
    src/object/information/BinaryCmd.h
    src/object/information/BinaryInfo.h
    src/object/information/DoubleCmd.h
    src/object/information/DoubleInfo.h
    src/object/information/EquipmentProtectionEvent.h
    src/object/information/EquipmentProtectionOutputCircuitEvent.h
    src/object/information/EquipmentProtectionStartEvents.h
    src/object/information/IntegratedTotalInfo.h
    src/object/information/NormalizedCmd.h
    src/object/information/NormalizedInfo.h
    src/object/information/ScaledCmd.h
    src/object/information/ScaledInfo.h
    src/object/information/ShortCmd.h
    src/object/information/ShortInfo.h
    src/object/information/SingleCmd.h
    src/object/information/SingleInfo.h
    src/object/information/StatusWithChangeDetection.h
    src/object/information/StepCmd.h
    src/object/information/StepInfo.h
    src/transformer/Type.h
    src/transformer/Information.h
    src/object/DataPoint.h
    src/object/Station.h
    src/remote/Helper.h
    src/remote/Helper.cpp
    src/remote/TransportSecurity.h
    src/remote/Selection.h
    src/remote/Connection.h
    src/remote/message/IMessageInterface.h
    src/remote/message/IncomingMessage.h
    src/remote/message/OutgoingMessage.h
    src/remote/message/Batch.h
    src/tasks/Task.h
    src/tasks/Executor.h
    src/Client.h
    src/Server.h
    src/debug.cpp
    src/enums.cpp
    src/types.cpp
    src/object/DateTime.cpp
    src/object/DateTimePy.cpp
    src/object/information/IInformation.cpp
    src/object/information/Generic.cpp
    src/object/information/ICommand.cpp
    src/object/information/BinaryCmd.cpp
    src/object/information/BinaryInfo.cpp
    src/object/information/DoubleCmd.cpp
    src/object/information/DoubleInfo.cpp
    src/object/information/EquipmentProtectionEvent.cpp
    src/object/information/EquipmentProtectionOutputCircuitEvent.cpp
    src/object/information/EquipmentProtectionStartEvents.cpp
    src/object/information/IntegratedTotalInfo.cpp
    src/object/information/NormalizedCmd.cpp
    src/object/information/NormalizedInfo.cpp
    src/object/information/ScaledCmd.cpp
    src/object/information/ScaledInfo.cpp
    src/object/information/ShortCmd.cpp
    src/object/information/ShortInfo.cpp
    src/object/information/SingleCmd.cpp
    src/object/information/SingleInfo.cpp
    src/object/information/StatusWithChangeDetection.cpp
    src/object/information/StepCmd.cpp
    src/object/information/StepInfo.cpp
    src/transformer/Type.cpp
    src/transformer/Information.cpp
    src/object/information/python.cpp
    src/object/DataPoint.cpp
    src/object/DataPointPy.cpp
    src/object/Station.cpp
    src/object/StationPy.cpp
    src/remote/TransportSecurity.cpp
    src/remote/TransportSecurityPy.cpp
    src/remote/Selection.cpp
    src/remote/Connection.cpp
    src/remote/ConnectionPy.cpp
    src/remote/message/IncomingMessage.cpp
    src/remote/message/IncomingMessagePy.cpp
    src/remote/message/OutgoingMessage.cpp
    src/remote/message/Batch.cpp
    src/remote/message/BatchPy.cpp
    src/tasks/Executor.cpp
    src/Client.cpp
    src/ClientPy.cpp
    src/Server.cpp
    src/ServerPy.cpp
    src/python.cpp)

pybind11_add_module(_c104 MODULE OPT_SIZE ${c104_SOURCES})

if(c104_PRIVATE_LIBRARIES)
  target_link_libraries(_c104 PRIVATE ${c104_PRIVATE_LIBRARIES})
endif()

target_compile_definitions(_c104 PRIVATE ${c104_PRIVATE_DEFINITIONS})

# ##############################################################################
# Tests with Catch2
# ##############################################################################

if(EXISTS "${PROJECT_SOURCE_DIR}/depends/catch/src"
   AND EXISTS "${PROJECT_SOURCE_DIR}/tests/main.cpp")
  message(STATUS "Add catch2 and tests")

  add_executable(
    c104_tests
    ${c104_SOURCES} tests/test_object_datetime.cpp
    tests/test_object_datapoint.cpp tests/test_object_station.cpp
    tests/test_remote_message_batch.cpp tests/main.cpp)

  if(TARGET c104_tests)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

    add_subdirectory(depends/catch)
    list(APPEND c104_tests_PRIVATE_LIBRARIES Catch2::Catch2)

    if(c104_tests_PRIVATE_LIBRARIES)
      target_link_libraries(c104_tests PRIVATE ${c104_tests_PRIVATE_LIBRARIES})
    endif()

    include(CTest)
    include(Catch)
    catch_discover_tests(c104_tests)

  endif()

  add_executable(c104_test_client ${c104_SOURCES} src/main_client.cpp)

  if(c104_tests_PRIVATE_LIBRARIES)
    target_link_libraries(c104_test_client
                          PRIVATE ${c104_tests_PRIVATE_LIBRARIES})
  endif()

  add_executable(c104_test_server ${c104_SOURCES} src/main_server.cpp)

  if(c104_tests_PRIVATE_LIBRARIES)
    target_link_libraries(c104_test_server
                          PRIVATE ${c104_tests_PRIVATE_LIBRARIES})
  endif()
else()
  message(STATUS "Skip catch2 and tests")
endif()
