cmake_minimum_required(VERSION 3.22)

project(netp2 VERSION 0.1.0 LANGUAGES CXX)

if(POLICY CMP0167)
	cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NETP2_BUILD_TESTS "Build tests" ON)
option(NETP2_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(NETP2_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(NETP2_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(warnings)
include(sanitizers)

enable_testing()

find_package(Boost 1.74 REQUIRED)
find_package(Threads REQUIRED)

# llhttp for HTTP/1.1 parsing - fetch from source
include(FetchContent)

# Configure llhttp to build static library
set(LLHTTP_BUILD_SHARED_LIBS OFF CACHE BOOL "Build llhttp shared library" FORCE)
set(LLHTTP_BUILD_STATIC_LIBS ON CACHE BOOL "Build llhttp static library" FORCE)

FetchContent_Declare(
    llhttp
    URL "https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.4.3.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(llhttp)

message(STATUS "llhttp fetched from source (v9.4.3)")

add_subdirectory(src)
add_subdirectory(tests)
