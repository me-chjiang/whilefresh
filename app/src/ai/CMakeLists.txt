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
    ingredient-classifier.cpp
    ingredient-classifier.h
    preference-summarizer.cpp
    preference-summarizer.h
    recorded-chat-client.cpp
    recorded-chat-client.h
    system-prompt-builder.cpp
    system-prompt-builder.h
)

# Checked in app/CMakeLists.txt: nothing beyond core and Qt::Network.
target_link_libraries(whilefresh-ai PUBLIC whilefresh-core Qt6::Network)

# ADR-5: keys come from the environment (scripts read them from the macOS keychain and
# ~/.config/lexmount/config.yaml) and are written only into the build tree. They are deliberately
# not cache variables, so they never land in CMakeCache.txt; reconfiguring without the variables
# produces a key-less build. The DeepSeek key and the Lexmount pair are injected together.
function(whilefresh_read_build_secret env_name out_var pattern)
    set(_value "$ENV{${env_name}}")
    if(NOT _value MATCHES "^${pattern}*$")
        message(FATAL_ERROR "${env_name} contains characters outside [${pattern}]")
    endif()
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

whilefresh_read_build_secret(WHILEFRESH_DEEPSEEK_API_KEY _deepseek_api_key "[A-Za-z0-9_-]")
whilefresh_read_build_secret(WHILEFRESH_LEXMOUNT_API_KEY _lexmount_api_key "[A-Za-z0-9_-]")
whilefresh_read_build_secret(WHILEFRESH_LEXMOUNT_PROJECT_ID _lexmount_project_id "[A-Za-z0-9_-]")

foreach(_entry "DeepSeek API key|_deepseek_api_key" "Lexmount API key|_lexmount_api_key"
        "Lexmount project id|_lexmount_project_id")
    string(REPLACE "|" ";" _entry "${_entry}")
    list(GET _entry 0 _label)
    list(GET _entry 1 _var)
    if(${_var})
        message(STATUS "${_label}: injected")
    elseif(DEFINED ENV{WHILEFRESH_KEY_FROM_SCRIPT})
        message(STATUS "${_label}: not set, the matching services will report they are not configured")
    else()
        # A re-run triggered by ninja does not see the values the build script exported earlier.
        message(WARNING "${_label}: not set and configure was not started by a scripts/build-*.sh "
            "script (e.g. ninja re-ran CMake). This is a key-less build (本次为无密钥构建). "
            "Re-run the build script to inject the keys.")
    endif()
endforeach()
set(WHILEFRESH_GENERATED_DEEPSEEK_API_KEY "${_deepseek_api_key}")
set(WHILEFRESH_GENERATED_LEXMOUNT_API_KEY "${_lexmount_api_key}")
set(WHILEFRESH_GENERATED_LEXMOUNT_PROJECT_ID "${_lexmount_project_id}")
configure_file(api-config.h.in "${CMAKE_BINARY_DIR}/generated/api-config.h" @ONLY)
unset(WHILEFRESH_GENERATED_DEEPSEEK_API_KEY)
unset(WHILEFRESH_GENERATED_LEXMOUNT_API_KEY)
unset(WHILEFRESH_GENERATED_LEXMOUNT_PROJECT_ID)
unset(_deepseek_api_key)
unset(_lexmount_api_key)
unset(_lexmount_project_id)

target_include_directories(whilefresh-ai PRIVATE "${CMAKE_BINARY_DIR}")
