# Tools/Binaries CMakeLists.txt

if(ENABLE_BINARY)
    # Example apps
    add_executable(avioflow-offline-load-audio avioflow-offline-load-audio.cpp)
    target_link_libraries(avioflow-offline-load-audio PRIVATE avioflow)

    add_executable(avioflow-online-load-audio avioflow-online-load-audio.cpp)
    target_link_libraries(avioflow-online-load-audio PRIVATE avioflow)

    if(ENABLE_WASAPI AND WIN32)
        add_executable(avioflow-wasapi-capture avioflow-wasapi-capture.cpp)
        target_link_libraries(avioflow-wasapi-capture PRIVATE avioflow)
    endif()

    add_executable(avioflow-microphone-capture avioflow-microphone-capture.cpp)
    target_link_libraries(avioflow-microphone-capture PRIVATE avioflow)
endif()

if(BUILD_TESTS)
    add_executable(ffmpeg-decoder-offline-load-test decoder-offline-load-test.cpp)
    target_link_libraries(ffmpeg-decoder-offline-load-test PRIVATE avioflow)

    add_executable(ffmpeg-decoder-online-load-test decoder-online-load-test.cpp)
    target_link_libraries(ffmpeg-decoder-online-load-test PRIVATE avioflow)

    add_executable(ffmpeg-decoder-resample-test decoder-resample-test.cpp)
    target_link_libraries(ffmpeg-decoder-resample-test PRIVATE avioflow)

    add_executable(ffmpeg-device-list-test device-list-test.cpp)
    target_link_libraries(ffmpeg-device-list-test PRIVATE avioflow)

    add_executable(ffmpeg-info-test ffmpeg-info-test.cpp)
    target_link_libraries(ffmpeg-info-test PRIVATE avioflow)

    add_executable(ffmpeg-encoder-offline-save-test encoder-offline-save-test.cpp)
    target_link_libraries(ffmpeg-encoder-offline-save-test PRIVATE avioflow)

    add_executable(ffmpeg-decoder-seek-test decoder-seek-test.cpp)
    target_link_libraries(ffmpeg-decoder-seek-test PRIVATE avioflow)

    add_executable(ffmpeg-resample-test resample-test.cpp)
    target_link_libraries(ffmpeg-resample-test PRIVATE avioflow)

    if(BUILD_TESTING)
        add_executable(ffmpeg-decoder-validation-test decoder-validation-test.cpp)
        target_link_libraries(ffmpeg-decoder-validation-test PRIVATE avioflow)

        add_test(NAME decoder-validation COMMAND ffmpeg-decoder-validation-test)
        add_test(NAME decoder-online COMMAND ffmpeg-decoder-online-load-test)
        add_test(NAME decoder-resample COMMAND ffmpeg-decoder-resample-test)
        add_test(NAME decoder-seek COMMAND ffmpeg-decoder-seek-test)
        add_test(NAME resample COMMAND ffmpeg-resample-test)
        set_tests_properties(decoder-online decoder-resample decoder-seek PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endif()
endif()
