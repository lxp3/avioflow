# Tests CMakeLists.txt

# Example/Test apps
add_executable(avioflow-offline-load-audio avioflow-offline-load-audio.cpp)
target_link_libraries(avioflow-offline-load-audio PRIVATE avioflow)

add_executable(avioflow-online-load-audio avioflow-online-load-audio.cpp)
target_link_libraries(avioflow-online-load-audio PRIVATE avioflow)

if(ENABLE_WASAPI AND WIN32)
    add_executable(avioflow-wasapi-capture avioflow-wasapi-capture.cpp)
    target_link_libraries(avioflow-wasapi-capture PRIVATE avioflow)
endif()

# Unit tests - Decoder tests
add_executable(ffmpeg-decoder-load-test ffmpeg/decoder/load-test.cpp)
target_include_directories(ffmpeg-decoder-load-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-decoder-load-test PRIVATE avioflow)

add_executable(ffmpeg-decoder-resample-test ffmpeg/decoder/resample-test.cpp)
target_include_directories(ffmpeg-decoder-resample-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-decoder-resample-test PRIVATE avioflow)

add_executable(ffmpeg-device-list-test ffmpeg/device-list-test.cpp)
target_include_directories(ffmpeg-device-list-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-device-list-test PRIVATE avioflow)

# Copy FFmpeg DLLs and avioflow.dll to test executables - Windows Shared only
if(BUILD_SHARED_LIBS AND WIN32)
    set(TEST_TARGETS 
        avioflow-offline-load-audio 
        avioflow-online-load-audio 
        ffmpeg-decoder-load-test 
        ffmpeg-decoder-resample-test
        ffmpeg-device-list-test
    )
    if(ENABLE_WASAPI AND WIN32)
        list(APPEND TEST_TARGETS avioflow-wasapi-capture)
    endif()
    foreach(target IN LISTS TEST_TARGETS)
        # Copy FFmpeg DLLs
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${FFMPEG_BIN_DIR}"
                "$<TARGET_FILE_DIR:${target}>"
        )
        # Copy avioflow.dll
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:avioflow>"
                "$<TARGET_FILE_DIR:${target}>"
        )
    endforeach()
endif()
