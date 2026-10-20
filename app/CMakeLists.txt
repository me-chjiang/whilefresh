cmake_minimum_required(VERSION 3.21)

project(WhileFresh VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Sql)
qt_standard_project_setup(REQUIRES 6.8)

# The module directory "WhileFresh" would collide with the "whilefresh" executable on
# case-insensitive macOS filesystems.
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# Layer libraries. Dependency direction: core <- data/recognition/platform <- ui <- app.
add_subdirectory(src/core)
add_subdirectory(src/data)
add_subdirectory(src/recognition)
add_subdirectory(src/platform)
add_subdirectory(src/ui)

# core must stay testable on the desktop without GUI, network or other layers (FHS.md).
get_target_property(_core_links whilefresh-core LINK_LIBRARIES)
foreach(_lib IN LISTS _core_links)
    if(NOT _lib MATCHES "^Qt6?::Core$")
        message(FATAL_ERROR "whilefresh-core may only link Qt::Core, found: ${_lib}")
    endif()
endforeach()

qt_add_executable(whilefresh src/main.cpp)

qt_add_qml_module(whilefresh
    URI WhileFresh
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/ImportDebugPanel.qml
)

target_link_libraries(whilefresh PRIVATE
    whilefresh-ui
    whilefresh-recognition
    whilefresh-data
    whilefresh-platform
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

target_compile_definitions(whilefresh PRIVATE
    $<$<CONFIG:Debug>:WHILEFRESH_TLS_SMOKE>
    $<$<CONFIG:Debug>:WHILEFRESH_DEBUG_TOOLS>
)

if(ANDROID)
    set_target_properties(whilefresh PROPERTIES
        QT_ANDROID_PACKAGE_NAME io.github.mechjiang.whilefresh
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 34
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    )

    # Qt's Android TLS backend loads OpenSSL at runtime but the kit ships without it (ADR-3).
    set(WHILEFRESH_ANDROID_OPENSSL_DIR "$ENV{WHILEFRESH_ANDROID_OPENSSL_DIR}" CACHE PATH
        "Checkout of KDAB/android_openssl (see scripts/fetch-deps.sh)")
    if(NOT WHILEFRESH_ANDROID_OPENSSL_DIR)
        set(WHILEFRESH_ANDROID_OPENSSL_DIR "$ENV{HOME}/.cache/whilefresh/android_openssl" CACHE PATH
            "Checkout of KDAB/android_openssl (see scripts/fetch-deps.sh)" FORCE)
    endif()
    if(NOT EXISTS "${WHILEFRESH_ANDROID_OPENSSL_DIR}/android_openssl.cmake")
        message(FATAL_ERROR "android_openssl not found at ${WHILEFRESH_ANDROID_OPENSSL_DIR}; run scripts/fetch-deps.sh")
    endif()
    include("${WHILEFRESH_ANDROID_OPENSSL_DIR}/android_openssl.cmake")
    add_android_openssl_libraries(whilefresh)
else()
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(tools)
endif()
