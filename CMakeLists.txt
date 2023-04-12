cmake_minimum_required(VERSION 3.5)
project(c104)

# ##############################################################################
# COMPILE FLAGS
# ##############################################################################

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "ModulePath: ${CMAKE_MODULE_PATH}")

if(DEFINED ENV{TOOLCHAIN})
  message(ERROR "TOOLCHAIN: $ENV{TOOLCHAIN}")
endif()

# ##############################################################################
# pybind11 and Python3
# ##############################################################################

message(STATUS "Add Pybind11")
add_subdirectory(depends/pybind11)

# ##############################################################################
# lib60870-C 2.3.2
# ##############################################################################

message(STATUS "Add lib60870")
set(BUILD_COMMON ON)
set(BUILD_HAL ON)
set(WITH_MBEDTLS 1)
message(STATUS "Copy mbedtls")
file(
  COPY depends/mbedtls/library depends/mbedtls/include
       depends/mbedtls/CMakeLists.txt
  DESTINATION
    ${PROJECT_SOURCE_DIR}/depends/lib60870/lib60870-C/dependencies/mbedtls-2.28)
add_subdirectory(depends/lib60870/lib60870-C)

# ##############################################################################
# c104
# ##############################################################################

include_directories(
  src depends/lib60870/lib60870-C/src/inc/api
  depends/lib60870/lib60870-C/src/hal/inc
  depends/lib60870/lib60870-C/src/common/inc)

add_library(
  c104 MODULE
  src/enums.h
  src/enums.cpp
  src/types.cpp
  src/types.h
  src/module/Callback.h
  src/module/ScopedGilAcquire.h
  src/module/ScopedGilRelease.h
  src/module/GilAwareMutex.h
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
  src/object/DataPoint.cpp
  src/object/Station.cpp
  src/main.cpp)

target_link_libraries(c104 PRIVATE pybind11::module pybind11::windows_extras
                                   lib60870)
if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  target_link_libraries(c104 PRIVATE pybind11::lto)
endif()

pybind11_extension(c104)
if(NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
  pybind11_strip(c104)
endif()

set_target_properties(
  c104
  PROPERTIES CXX_VISIBILITY_PRESET "hidden"
             CUDA_VISIBILITY_PRESET "hidden"
             PREFIX "${PYTHON_MODULE_PREFIX}"
             SUFFIX "${PYTHON_MODULE_EXTENSION}")

# ##############################################################################
# Tests with Catch2
# ##############################################################################

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

add_subdirectory(depends/catch)

add_executable(
  c104_tests
  src/enums.h
  src/enums.cpp
  src/types.cpp
  src/types.h
  src/module/Callback.h
  src/module/ScopedGilAcquire.h
  src/module/ScopedGilRelease.h
  src/module/GilAwareMutex.h
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
  src/object/DataPoint.cpp
  src/object/Station.cpp
  tests/test_object_datapoint.cpp
  tests/test_object_station.cpp
  tests/main.cpp)
target_link_libraries(
  c104_tests PRIVATE pybind11::embed pybind11::windows_extras lib60870
                     Catch2::Catch2)

include(CTest)
include(Catch)
catch_discover_tests(c104_tests)

add_executable(
  c104_test_client
  src/enums.h
  src/enums.cpp
  src/types.cpp
  src/types.h
  src/module/Callback.h
  src/module/ScopedGilAcquire.h
  src/module/ScopedGilRelease.h
  src/module/GilAwareMutex.h
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
  src/object/DataPoint.cpp
  src/object/Station.cpp
  src/main_client.cpp)
target_link_libraries(c104_test_client
                      PRIVATE pybind11::embed pybind11::windows_extras lib60870)
