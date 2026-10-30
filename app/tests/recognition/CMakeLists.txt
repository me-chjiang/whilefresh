get_filename_component(_test_data_dir "${PROJECT_SOURCE_DIR}/../test-data" ABSOLUTE)

function(whilefresh_add_recognition_test name)
    whilefresh_add_test(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE whilefresh-recognition)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../ai")
    target_compile_definitions(${name} PRIVATE
        WHILEFRESH_TEST_DATA_DIR="${_test_data_dir}"
        WHILEFRESH_DEMO_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets/demo"
    )
endfunction()

whilefresh_add_recognition_test(image-preprocessor-test image-preprocessor-test.cpp)
whilefresh_add_recognition_test(request-builder-test request-builder-test.cpp)
whilefresh_add_recognition_test(response-parser-test response-parser-test.cpp)
whilefresh_add_recognition_test(recorded-recognizer-test recorded-recognizer-test.cpp)
whilefresh_add_recognition_test(deepseek-recognizer-test deepseek-recognizer-test.cpp ../ai/fake-http-server.h)
whilefresh_add_recognition_test(vocabulary-test vocabulary-test.cpp)
