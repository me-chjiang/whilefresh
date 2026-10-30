get_filename_component(_test_data_dir "${PROJECT_SOURCE_DIR}/../test-data" ABSOLUTE)

function(whilefresh_add_ai_test name)
    whilefresh_add_test(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE whilefresh-ai)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${name} PRIVATE
        WHILEFRESH_TEST_DATA_DIR="${_test_data_dir}"
        WHILEFRESH_DEMO_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets/demo"
    )
endfunction()

whilefresh_add_ai_test(deepseek-chat-client-test deepseek-chat-client-test.cpp ai-test-support.h fake-http-server.h)
whilefresh_add_ai_test(chat-recording-test chat-recording-test.cpp ai-test-support.h fake-http-server.h)
whilefresh_add_ai_test(chat-types-test chat-types-test.cpp ai-test-support.h)
whilefresh_add_ai_test(system-prompt-builder-test system-prompt-builder-test.cpp)
whilefresh_add_ai_test(ingredient-classifier-test ingredient-classifier-test.cpp ai-test-support.h)
whilefresh_add_ai_test(preference-summarizer-test preference-summarizer-test.cpp ai-test-support.h)
