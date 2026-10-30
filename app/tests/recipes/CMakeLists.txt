get_filename_component(_test_data_dir "${PROJECT_SOURCE_DIR}/../test-data" ABSOLUTE)

function(whilefresh_add_recipes_test name)
    whilefresh_add_test(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE whilefresh-recipes)
    target_compile_definitions(${name} PRIVATE
        WHILEFRESH_RECIPE_SAMPLES_DIR="${_test_data_dir}/recipes/xiachufang"
    )
endfunction()

whilefresh_add_recipes_test(xiachufang-yaml-parser-test xiachufang-yaml-parser-test.cpp)
whilefresh_add_recipes_test(recorded-recipe-source-test recorded-recipe-source-test.cpp)

if(WHILEFRESH_XIACHUFANG_ADAPTER)
    whilefresh_add_recipes_test(process-recipe-source-test process-recipe-source-test.cpp)
endif()
