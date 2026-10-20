add_library(whilefresh-recognition STATIC
    api-key.cpp
    api-key.h
    tls-smoke.cpp
    tls-smoke.h
)

target_link_libraries(whilefresh-recognition PUBLIC whilefresh-core Qt6::Network)

# ADR-5: the key comes from the environment (scripts read it from the macOS keychain) and is
# written only into the build tree. It is deliberately not a cache variable, so it never lands
# in CMakeCache.txt; reconfiguring without the variable produces a key-less build.
set(_deepseek_api_key "$ENV{WHILEFRESH_DEEPSEEK_API_KEY}")
if(NOT _deepseek_api_key MATCHES "^[A-Za-z0-9_-]*$")
    message(FATAL_ERROR "WHILEFRESH_DEEPSEEK_API_KEY contains characters outside [A-Za-z0-9_-]")
endif()
if(_deepseek_api_key)
    message(STATUS "DeepSeek API key: injected")
else()
    message(STATUS "DeepSeek API key: not set, recognition will report it is not configured")
endif()
set(WHILEFRESH_GENERATED_DEEPSEEK_API_KEY "${_deepseek_api_key}")
configure_file(api-config.h.in "${CMAKE_BINARY_DIR}/generated/api-config.h" @ONLY)
unset(WHILEFRESH_GENERATED_DEEPSEEK_API_KEY)
unset(_deepseek_api_key)

target_include_directories(whilefresh-recognition PRIVATE "${CMAKE_BINARY_DIR}")
