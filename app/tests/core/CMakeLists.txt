function(whilefresh_add_core_test name)
    whilefresh_add_test(${name} ${ARGN} core-test-support.h)
    target_link_libraries(${name} PRIVATE whilefresh-core)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

whilefresh_add_core_test(date-text-test date-text-test.cpp)
whilefresh_add_core_test(shelf-life-test shelf-life-test.cpp)
whilefresh_add_core_test(freshness-test freshness-test.cpp)
whilefresh_add_core_test(household-rules-test household-rules-test.cpp)
whilefresh_add_core_test(quantity-test quantity-test.cpp)
whilefresh_add_core_test(fridge-groups-test fridge-groups-test.cpp)
whilefresh_add_core_test(resumable-job-test resumable-job-test.cpp)

get_filename_component(_test_data_dir "${PROJECT_SOURCE_DIR}/../test-data" ABSOLUTE)
whilefresh_add_core_test(catalog-test catalog-test.cpp)
whilefresh_add_core_test(vocabulary-resolver-test vocabulary-resolver-test.cpp)
foreach(_test catalog-test vocabulary-resolver-test)
    target_compile_definitions(${_test} PRIVATE
        WHILEFRESH_VOCABULARY_FILE="${PROJECT_SOURCE_DIR}/assets/data/vocabulary.json"
        WHILEFRESH_RESOLVE_CASES_FILE="${_test_data_dir}/vocabulary/resolve-cases.json"
    )
endforeach()
