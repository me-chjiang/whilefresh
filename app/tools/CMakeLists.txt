# Desktop-only developer tools; not registered with ctest.
add_executable(recognize-cli recognize-cli.cpp)
target_link_libraries(recognize-cli PRIVATE whilefresh-recognition)
