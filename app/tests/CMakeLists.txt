find_package(Qt6 REQUIRED COMPONENTS Test)

function(whilefresh_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

whilefresh_add_test(app-version-test core/app-version-test.cpp)
target_link_libraries(app-version-test PRIVATE whilefresh-core)
target_compile_definitions(app-version-test PRIVATE WHILEFRESH_EXPECTED_VERSION="${PROJECT_VERSION}")
