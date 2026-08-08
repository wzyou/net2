add_library(netp2_core
    observability/metrics.cpp
    protocol/http1_codec.cpp
    resilience/circuit_breaker.cpp
    resilience/rate_limiter.cpp
    routing/route_snapshot.cpp
    runtime/gateway_runtime.cpp
    runtime/spine.cpp
    upstream/connection_pool.cpp
)

target_include_directories(netp2_core PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

target_compile_features(netp2_core PUBLIC cxx_std_20)
target_compile_definitions(netp2_core PUBLIC BOOST_ERROR_CODE_HEADER_ONLY)

# Link llhttp (fetched from source)
target_link_libraries(netp2_core PRIVATE llhttp_static)

if(TARGET Boost::headers)
    target_link_libraries(netp2_core PUBLIC Boost::headers Threads::Threads)
else()
    target_link_libraries(netp2_core PUBLIC Boost::boost Threads::Threads)
endif()

netp2_enable_warnings(netp2_core)
netp2_enable_sanitizers(netp2_core)

add_executable(netp2_gateway
    app/main.cpp
)

target_link_libraries(netp2_gateway PRIVATE netp2_core)
netp2_enable_warnings(netp2_gateway)
netp2_enable_sanitizers(netp2_gateway)
