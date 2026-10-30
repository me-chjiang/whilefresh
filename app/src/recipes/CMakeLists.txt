add_library(whilefresh-recipes STATIC
    recipe-source.h
    recipe-types.cpp
    recipe-types.h
    recipes-log.h
    recorded-recipe-source.cpp
    recorded-recipe-source.h
    xiachufang-yaml-parser.cpp
    xiachufang-yaml-parser.h
)

# Checked in app/CMakeLists.txt: nothing beyond core, Qt::Core and yaml-cpp.
target_link_libraries(whilefresh-recipes PUBLIC whilefresh-core)

# yaml-cpp parses the source's YAML contract (the external source pins the same commit). It is
# built from the checkout scripts/fetch-deps.sh creates; configure never touches the network.
set(WHILEFRESH_YAML_CPP_DIR "$ENV{WHILEFRESH_YAML_CPP_DIR}" CACHE PATH
    "Checkout of jbeder/yaml-cpp (see scripts/fetch-deps.sh)")
if(NOT WHILEFRESH_YAML_CPP_DIR)
    set(WHILEFRESH_YAML_CPP_DIR "$ENV{HOME}/.cache/whilefresh/yaml-cpp" CACHE PATH
        "Checkout of jbeder/yaml-cpp (see scripts/fetch-deps.sh)" FORCE)
endif()
if(NOT EXISTS "${WHILEFRESH_YAML_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "yaml-cpp not found at ${WHILEFRESH_YAML_CPP_DIR}; run scripts/fetch-deps.sh")
endif()
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${WHILEFRESH_YAML_CPP_DIR}" "${CMAKE_BINARY_DIR}/yaml-cpp" EXCLUDE_FROM_ALL)
target_link_libraries(whilefresh-recipes PRIVATE yaml-cpp::yaml-cpp)

# Runs the external recipe-source CLI, so it exists on the desktop only and is off by default.
option(WHILEFRESH_XIACHUFANG_ADAPTER "Build ProcessRecipeSource, the adapter for recipe-sources-xiachufang-api" OFF)
if(WHILEFRESH_XIACHUFANG_ADAPTER AND NOT ANDROID)
    target_sources(whilefresh-recipes PRIVATE process-recipe-source.cpp process-recipe-source.h)
    target_compile_definitions(whilefresh-recipes PUBLIC WHILEFRESH_XIACHUFANG_ADAPTER)
endif()
