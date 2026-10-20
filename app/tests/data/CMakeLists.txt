function(whilefresh_add_data_test name)
    whilefresh_add_test(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE whilefresh-data)
endfunction()

whilefresh_add_data_test(database-test database-test.cpp)
whilefresh_add_data_test(store-test store-test.cpp)
whilefresh_add_data_test(builtin-data-test builtin-data-test.cpp)
