add_library(whilefresh-recognition STATIC
    deepseek-recognizer.cpp
    deepseek-recognizer.h
    demo-recognizer.h
    image-preprocessor.cpp
    image-preprocessor.h
    prompt-templates.h
    recognition-log.h
    recognition-service.h
    recognition-types.cpp
    recognition-types.h
    recorded-recognizer.cpp
    recorded-recognizer.h
    recording.cpp
    recording.h
    replay-recognizer.h
    request-builder.cpp
    request-builder.h
    response-parser.cpp
    response-parser.h
    tls-smoke.cpp
    tls-smoke.h
)

target_link_libraries(whilefresh-recognition PUBLIC whilefresh-ai Qt6::Gui Qt6::Network)

# Demo-mode presets (DemoRecognizer::builtInDirectory()); T10 adds more files to assets/demo.
file(GLOB _demo_presets CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/demo/*.json")
qt_add_resources(whilefresh-recognition whilefresh-demo-presets
    PREFIX "/whilefresh/demo"
    BASE "${PROJECT_SOURCE_DIR}/assets/demo"
    FILES ${_demo_presets}
)
unset(_demo_presets)
