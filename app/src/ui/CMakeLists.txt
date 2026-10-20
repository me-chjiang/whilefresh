add_library(whilefresh-ui STATIC
    app-info.cpp
    app-info.h
)

target_link_libraries(whilefresh-ui PUBLIC
    whilefresh-core
    whilefresh-data
    whilefresh-recognition
    whilefresh-platform
)
