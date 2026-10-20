add_library(whilefresh-platform STATIC
    bridge-event.cpp
    bridge-event.h
    captured-at.cpp
    captured-at.h
    import-files.cpp
    import-files.h
    import-payload.cpp
    import-payload.h
    platform-info.cpp
    platform-info.h
)

target_link_libraries(whilefresh-platform PUBLIC whilefresh-core)
