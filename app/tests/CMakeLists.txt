find_package(Qt6 REQUIRED COMPONENTS Test)

function(whilefresh_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

whilefresh_add_test(app-version-test core/app-version-test.cpp)
target_link_libraries(app-version-test PRIVATE whilefresh-core)
target_compile_definitions(app-version-test PRIVATE WHILEFRESH_EXPECTED_VERSION="${PROJECT_VERSION}")

add_subdirectory(core)
add_subdirectory(ai)
add_subdirectory(recipes)
add_subdirectory(data)
add_subdirectory(recognition)

whilefresh_add_test(captured-at-test platform/captured-at-test.cpp)
target_link_libraries(captured-at-test PRIVATE whilefresh-platform)

whilefresh_add_test(file-key-vault-test platform/file-key-vault-test.cpp)
target_link_libraries(file-key-vault-test PRIVATE whilefresh-platform)

whilefresh_add_test(import-payload-test platform/import-payload-test.cpp)
target_link_libraries(import-payload-test PRIVATE whilefresh-platform)

whilefresh_add_test(platform-services-test platform/platform-services-test.cpp)
target_link_libraries(platform-services-test PRIVATE whilefresh-platform)

add_subdirectory(ui)
