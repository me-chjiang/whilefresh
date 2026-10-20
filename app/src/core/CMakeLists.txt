add_library(whilefresh-core STATIC
    app-version.cpp
    app-version.h
    catalog.cpp
    catalog.h
    clock.h
    date-text.cpp
    date-text.h
    freshness.cpp
    freshness.h
    fridge-item.cpp
    fridge-item.h
    import-duplicates.cpp
    import-duplicates.h
    pantry.cpp
    pantry.h
    recipe.cpp
    recipe.h
    shelf-life.cpp
    shelf-life.h
    shopping-list.cpp
    shopping-list.h
    storage.cpp
    storage.h
    texts.h
)

target_include_directories(whilefresh-core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(whilefresh-core PRIVATE WHILEFRESH_VERSION="${PROJECT_VERSION}")
target_link_libraries(whilefresh-core PUBLIC Qt6::Core)
