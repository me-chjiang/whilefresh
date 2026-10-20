add_library(whilefresh-data STATIC
    sqlite-support.cpp
    sqlite-support.h
)

target_link_libraries(whilefresh-data PUBLIC whilefresh-core Qt6::Sql)
