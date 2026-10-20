whilefresh_add_test(application-service-test application-service-test.cpp)
target_link_libraries(application-service-test PRIVATE whilefresh-ui)
target_compile_definitions(application-service-test PRIVATE
    WHILEFRESH_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/../test-data"
)
