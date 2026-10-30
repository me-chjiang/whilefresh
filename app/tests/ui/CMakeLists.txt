whilefresh_add_test(application-service-test application-service-test.cpp)
target_link_libraries(application-service-test PRIVATE whilefresh-ui)
target_compile_definitions(application-service-test PRIVATE
    WHILEFRESH_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/../test-data"
)

whilefresh_add_test(settings-model-test settings-model-test.cpp)
target_link_libraries(settings-model-test PRIVATE whilefresh-ui)
whilefresh_add_test(web-recipe-service-test web-recipe-service-test.cpp)
target_link_libraries(web-recipe-service-test PRIVATE whilefresh-ui)
