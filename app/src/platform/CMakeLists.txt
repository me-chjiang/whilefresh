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
    platform-services.cpp
    platform-services.h
)

target_link_libraries(whilefresh-platform PUBLIC whilefresh-core Qt6::Gui)

if(ANDROID)
    target_sources(whilefresh-platform PRIVATE
        android/android-platform-services.cpp
        android/android-platform-services.h
    )
else()
    # QFileDialog stands in for the system picker and camera; it needs a QApplication (main.cpp).
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    target_sources(whilefresh-platform PRIVATE
        desktop/desktop-platform-services.cpp
        desktop/desktop-platform-services.h
    )
    target_link_libraries(whilefresh-platform PUBLIC Qt6::Widgets)
endif()
