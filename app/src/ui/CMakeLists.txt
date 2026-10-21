add_library(whilefresh-ui STATIC
    app-info.cpp
    app-info.h
    app-model.cpp
    app-model.h
    kitchen-service.cpp
    kitchen-service.h
    import-service.cpp
    import-service.h
)

target_link_libraries(whilefresh-ui PUBLIC
    whilefresh-core
    whilefresh-data
    whilefresh-recognition
    whilefresh-platform
)
