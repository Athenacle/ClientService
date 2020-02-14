cmake_minimum_required(VERSION 3.4)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

project(clientServiceServer CXX)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

if(UNIX)
  set(UNIX "Build ON UNIX" ON)
  set(ATHDNS_BUILD_ON_WINDOWS OFF)
else()
  if(WINDOWS)
    set(WINDOWS "Build ON Windows" ON)
  endif()
endif()


include(CheckCXXCompilerFlag)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckIncludeFileCXX)

check_function_exists(uname UNIX_HAVE_UNAME)


find_package(PostgreSQL)
include(FindLibPQXX)
include_directories(${LIBPQXX_INCLUDE_DIRS})

find_package(ZLIB)
include_directories(${ZLIB_INCLUDE_DIRS})

find_package(OpenSSL)
include_directories(${OpenSSL_INCLUDE_DIRS})

include_directories(${RAPIDJSON_INCLUDE_DIRS})

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  macro(CXX_COMPILER_CHECK_ADD)
    set(list_var "${ARGN}")
    foreach(flag IN LISTS list_var)
      string(TOUPPER ${flag} FLAG_NAME1)
      string(REPLACE "-" "_" FLAG_NAME2 ${FLAG_NAME1})
      string(CONCAT FLAG_NAME "COMPILER_SUPPORT_" ${FLAG_NAME2})
      check_cxx_compiler_flag(-${flag} ${FLAG_NAME})
      if (${${FLAG_NAME}})
        add_compile_options(-${flag})
      endif()
    endforeach()
  endmacro()

  CXX_COMPILER_CHECK_ADD(Wall
    Wextra
    Wpedantic
    Wduplicated-branches
    Wduplicated-cond
    Wlogical-op
    Wrestrict
    Wnull-dereference
    Wuseless-cast)

  check_cxx_compiler_flag(-fno-permissive COMPILER_SUPPORT_FNOPERMISSIVE)

  if (${COMPILER_SUPPORT_FNOPERMISSIVE})
    set(CMAKE_CXX_FLAGS "-fno-permissive ${CMAKE_CXX_FLAGS}")
  endif()

endif()

add_compile_options(-ggdb3)

find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ClientService/clientService.proto)

configure_file(ClientService/config.h.in
  ${CMAKE_BINARY_DIR}/config.h)
add_definitions(-DHAVE_CONFIG_H)

include_directories(${CMAKE_BINARY_DIR})

add_library(WindowsProtobufLib STATIC WindowsProtobufLib/protobufLib.cpp clientService.pb.cc WindowsProtobufLib/utils.cpp)

find_path(LIBUV_INCLUDE_DIR NAMES uv.h)
find_library(LIBUV_LIBRARIES NAMES uv libuv)

find_package(Threads REQUIRED)

include(ExternalProject)

ExternalProject_Add(
  gtest
  URL https://github.com/google/googletest/archive/release-1.10.0.zip
  DOWNLOAD_NO_PROGRESS ON
  PREFIX gtest
  INSTALL_COMMAND "")

enable_testing()

ExternalProject_Get_Property(gtest source_dir binary_dir)

if(${COMPILER_SUPPORT_NO_ZERO_AS_NULL})
  add_compile_options(-Wno-zero-as-null-pointer-constant)
endif()

add_library(libgtest IMPORTED STATIC GLOBAL)
add_dependencies(libgtest gtest)

set_target_properties(libgtest PROPERTIES
  IMPORTED_LOCATION ${binary_dir}/lib/libgtest.a
  IMPORTED_LINK_INTERFACE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

add_library(libgmock IMPORTED STATIC GLOBAL)
add_dependencies(libgmock gtest)

set_target_properties(libgmock PROPERTIES
  IMPORTED_LOCATION ${binary_dir}/lib/libgmock.a
  IMPORTED_LINK_INTERFACE_LIBRARIES  ${CMAKE_THREAD_LIBS_INIT})

include_directories(${source_dir}/googletest/include
  ${source_dir}/googlemock/include)

if(WIN32)
  list(APPEND LIBUV_LIBRARIES iphlpapi)
  list(APPEND LIBUV_LIBRARIES psapi)
  list(APPEND LIBUV_LIBRARIES userenv)
  list(APPEND LIBUV_LIBRARIES ws2_32)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBUV DEFAULT_MSG LIBUV_LIBRARIES LIBUV_INCLUDE_DIR)

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