cmake_minimum_required(VERSION 3.22)

project(netp2 VERSION 0.1.0 LANGUAGES CXX)

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

add_library(netp2_core INTERFACE)
target_include_directories(netp2_core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(netp2_core INTERFACE cxx_std_20)

add_executable(netp2_gateway
    src/app/main.cpp
)
target_link_libraries(netp2_gateway PRIVATE netp2_core)
netp2_enable_warnings(netp2_gateway)
netp2_enable_sanitizers(netp2_gateway)

if(NETP2_BUILD_TESTS)
    add_executable(netp2_smoke_test
        tests/unit/smoke_test.cpp
    )
    target_link_libraries(netp2_smoke_test PRIVATE netp2_core)
    netp2_enable_warnings(netp2_smoke_test)
    netp2_enable_sanitizers(netp2_smoke_test)

    add_test(NAME netp2_smoke_test COMMAND netp2_smoke_test)
endif()
