add_library(whilefresh-core STATIC
    app-version.cpp
    app-version.h
)

target_include_directories(whilefresh-core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(whilefresh-core PRIVATE WHILEFRESH_VERSION="${PROJECT_VERSION}")
target_link_libraries(whilefresh-core PUBLIC Qt6::Core)
