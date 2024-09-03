cmake_minimum_required(VERSION 3.15)
project(c104 LANGUAGES CXX)

# ##############################################################################
# COMPILE FLAGS
# ##############################################################################

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "ModulePath: ${CMAKE_MODULE_PATH}")

if(DEFINED ENV{TOOLCHAIN})
  message(STATUS "TOOLCHAIN: $ENV{TOOLCHAIN}")
endif()

# ##############################################################################
# pybind11 and Python3
# ##############################################################################

message(STATUS "Add Pybind11")
set(Python_FIND_STRATEGY "LOCATION")
set(Python_FIND_REGISTRY "LAST")
set(PYBIND11_FINDPYTHON ON)
add_subdirectory(depends/pybind11)
list(APPEND c104_tests_PRIVATE_LIBRARIES pybind11::embed)
if(MSVC)
  list(APPEND c104_tests_PRIVATE_LIBRARIES pybind11::windows_extras)
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
# lib60870-C 2.3.2
# ##############################################################################
message(STATUS "Patch lib60870")

# test apply the patch
execute_process(
  COMMAND git apply --check --ignore-whitespace ../lib60870.patch
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/depends/lib60870
  RESULT_VARIABLE GIT_APPLY_CHECK_RESULT
  ERROR_VARIABLE GIT_APPLY_CHECK_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# If the patch can be applied cleanly (i.e., it has not been applied yet), apply
# the patch
if(GIT_APPLY_CHECK_RESULT EQUAL "0")
  execute_process(
    COMMAND git apply --ignore-whitespace ../lib60870.patch
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/depends/lib60870
    RESULT_VARIABLE GIT_APPLY_RESULT
    ERROR_VARIABLE GIT_APPLY_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Check if the patch was applied successfully
  if(NOT GIT_APPLY_RESULT EQUAL "0")
    message(FATAL_ERROR "> Failed to apply patch: ${GIT_APPLY_ERROR}")
  else()
    message(STATUS "> Patch applied successfully.")
  endif()
else()
  message(STATUS "> Patch already applied or cannot be applied")
endif()

message(STATUS "Add lib60870")
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
    "${PROJECT_SOURCE_DIR}/depends/lib60870/lib60870-C/dependencies/mbedtls-2.28"
  )
  file(
    COPY depends/mbedtls/library depends/mbedtls/include
         depends/mbedtls/CMakeLists.txt
    DESTINATION
      ${PROJECT_SOURCE_DIR}/depends/lib60870/lib60870-C/dependencies/mbedtls-2.28
  )
else()
  if(EXISTS
     "${PROJECT_SOURCE_DIR}/depends/lib60870/lib60870-C/dependencies/mbedtls-2.28/library"
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
  depends/lib60870/lib60870-C/src/common/inc)

set(c104_SOURCES
    src/numbers.h
    src/enums.h
    src/enums.cpp
    src/types.cpp
    src/types.h
    src/module/Callback.h
    src/module/ScopedGilAcquire.h
    src/module/ScopedGilRelease.h
    src/module/GilAwareMutex.h
    src/object/Information.h
    src/object/DataPoint.h
    src/object/Station.h
    src/remote/Helper.h
    src/remote/Helper.cpp
    src/remote/TransportSecurity.cpp
    src/remote/TransportSecurity.h
    src/remote/Connection.cpp
    src/remote/Connection.h
    src/remote/message/IMessageInterface.h
    src/remote/message/IncomingMessage.cpp
    src/remote/message/IncomingMessage.h
    src/remote/message/OutgoingMessage.cpp
    src/remote/message/OutgoingMessage.h
    src/remote/message/PointCommand.cpp
    src/remote/message/PointCommand.h
    src/remote/message/PointMessage.cpp
    src/remote/message/PointMessage.h
    src/Client.cpp
    src/Client.h
    src/Server.cpp
    src/Server.h
    src/object/Information.cpp
    src/object/DataPoint.cpp
    src/object/Station.cpp
    src/python.cpp)

pybind11_add_module(c104 MODULE OPT_SIZE ${c104_SOURCES})

if(c104_PRIVATE_LIBRARIES)
  target_link_libraries(c104 PRIVATE ${c104_PRIVATE_LIBRARIES})
endif()

target_compile_definitions(c104 PRIVATE VERSION_INFO=\"${C104_VERSION_INFO}\")

# ##############################################################################
# Tests with Catch2
# ##############################################################################

if(EXISTS "${PROJECT_SOURCE_DIR}/depends/catch/src"
   AND EXISTS "${PROJECT_SOURCE_DIR}/tests/main.cpp")
  message(STATUS "Add catch2 and tests")

  add_executable(c104_tests ${c104_SOURCES} tests/test_object_datapoint.cpp
                            tests/test_object_station.cpp tests/main.cpp)

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
