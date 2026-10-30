add_library(whilefresh-data STATIC
    builtin-data.cpp
    builtin-data.h
    database.cpp
    database.h
    name-mappings.cpp
    name-mappings.h
    sqlite-support.cpp
    sqlite-support.h
    store.cpp
    store.h
)

target_link_libraries(whilefresh-data PUBLIC whilefresh-core Qt6::Sql)

# Read-only built-in data (BuiltinData::builtInDirectory()). shelf-life.json is optional.
file(GLOB _builtin_data CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/data/*.json")
qt_add_resources(whilefresh-data whilefresh-builtin-data
    PREFIX "/whilefresh/data"
    BASE "${PROJECT_SOURCE_DIR}/assets/data"
    FILES ${_builtin_data}
)
unset(_builtin_data)
