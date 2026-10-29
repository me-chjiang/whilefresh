add_library(whilefresh-ai STATIC
    ai-log.h
    chat-client.h
    chat-recording.cpp
    chat-recording.h
    chat-types.cpp
    chat-types.h
    api-keys.cpp
    api-keys.h
    deepseek-chat-client.cpp
    deepseek-chat-client.h
    recorded-chat-client.cpp
    recorded-chat-client.h
    system-prompt-builder.cpp
    system-prompt-builder.h
)

# Checked in app/CMakeLists.txt: nothing beyond core and Qt::Network.
target_link_libraries(whilefresh-ai PUBLIC whilefresh-core Qt6::Network)

# ADR-5: the key comes from the environment (scripts read it from the macOS keychain) and is
# written only into the build tree. It is deliberately not a cache variable, so it never lands
# in CMakeCache.txt; reconfiguring without the variable produces a key-less build.
set(_deepseek_api_key "$ENV{WHILEFRESH_DEEPSEEK_API_KEY}")
if(NOT _deepseek_api_key MATCHES "^[A-Za-z0-9_-]*$")
    message(FATAL_ERROR "WHILEFRESH_DEEPSEEK_API_KEY contains characters outside [A-Za-z0-9_-]")
endif()
if(_deepseek_api_key)
    message(STATUS "DeepSeek API key: injected")
elseif(DEFINED ENV{WHILEFRESH_KEY_FROM_SCRIPT})
    message(STATUS "DeepSeek API key: not set, AI services will report they are not configured")
else()
    # A re-run triggered by ninja does not see the key the build script exported earlier.
    message(WARNING "DeepSeek API key: not set and configure was not started by a scripts/build-*.sh "
        "script (e.g. ninja re-ran CMake). This is a key-less build (本次为无密钥构建). "
        "Re-run the build script to inject the key.")
endif()
set(WHILEFRESH_GENERATED_DEEPSEEK_API_KEY "${_deepseek_api_key}")
configure_file(api-config.h.in "${CMAKE_BINARY_DIR}/generated/api-config.h" @ONLY)
unset(WHILEFRESH_GENERATED_DEEPSEEK_API_KEY)
unset(_deepseek_api_key)

target_include_directories(whilefresh-ai PRIVATE "${CMAKE_BINARY_DIR}")
