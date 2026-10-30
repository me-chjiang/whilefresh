add_library(whilefresh-ui STATIC
    app-info.cpp
    app-info.h
    app-model.cpp
    app-model.h
    settings-model.cpp
    settings-model.h
    user-keys.cpp
    user-keys.h
    kitchen-service.cpp
    kitchen-service.h
    import-service.cpp
    import-service.h
    web-recipe-service.cpp
    web-recipe-service.h
)

target_link_libraries(whilefresh-ui PUBLIC
    whilefresh-core
    whilefresh-ai
    whilefresh-recipes
    whilefresh-data
    whilefresh-recognition
    whilefresh-platform
)
