add_library(whilefresh-platform STATIC
    platform-info.cpp
    platform-info.h
)

target_link_libraries(whilefresh-platform PUBLIC whilefresh-core)
