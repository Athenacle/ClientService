cmake_minimum_required(VERSION 3.4)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

project(clientServiceServer CXX)

if(UNIX)
  set(UNIX "Build ON UNIX" ON)
  set(ATHDNS_BUILD_ON_WINDOWS OFF)
else()
  if(WINDOWS)
    set(WINDOWS "Build ON Windows" ON)
  if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()
  endif()
endif()

find_package(PostgreSQL)
include(FindLibPQXX)
include_directories(${LIBPQXX_INCLUDE_DIRS})

find_package(ZLIB)
include_directories(${ZLIB_INCLUDE_DIRS})

find_package(OpenSSL)
include_directories(${OpenSSL_INCLUDE_DIRS})

find_package(RapidJSON)

include(CheckFunction)
include(CheckCXXCompiler)
include(Test)

find_package(LIBUV REQUIRED)

find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ClientService/clientService.proto)

configure_file(ClientService/config.h.in
  ${CMAKE_BINARY_DIR}/config.h)
add_definitions(-DHAVE_CONFIG_H)

include_directories(${CMAKE_BINARY_DIR})

add_library(WindowsProtobufLib STATIC WindowsProtobufLib/protobufLib.cpp clientService.pb.cc WindowsProtobufLib/utils.cpp)

include_directories(${CMAKE_SOURCE_DIR}/WindowsProtobufLib)
include_directories(${Protobuf_INCLUDE_DIR})
include_directories(${LIBUV_INCLUDE_DIR})

add_executable(protoTest ${CMAKE_SOURCE_DIR}/ProtobufLibraryTest/test.cpp)

target_link_libraries(protoTest ${ZLIB_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARY} libgtest ${Protobuf_LIBRARIES} WindowsProtobufLib)

add_executable(${PROJECT_NAME} 
  ${CMAKE_SOURCE_DIR}/ClientServiceServer/ClientServiceServer.cpp
  ${CMAKE_SOURCE_DIR}/ClientServiceServer/config.cpp
  ${CMAKE_SOURCE_DIR}/ClientServiceServer/client.cpp
  ${CMAKE_SOURCE_DIR}/ClientServiceServer/uv.cpp
  ${CMAKE_SOURCE_DIR}/ClientServiceServer/database.cpp)

target_link_libraries(${PROJECT_NAME} 
  ${ZLIB_LIBRARIES}
  ${OPENSSL_CRYPTO_LIBRARY} 
  ${PostgreSQL_LIBRARIES} 
  ${LIBPQXX_LIBRARIES} 
  ${LIBUV_LIBRARIES} 
  ${Protobuf_LIBRARIES} 
  WindowsProtobufLib)