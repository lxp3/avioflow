# Tests CMakeLists.txt

# Example/Test apps
add_executable(avioflow_offline_load avioflow-offline-load-audio.cpp)
target_link_libraries(avioflow_offline_load PRIVATE avioflow)

add_executable(avioflow_online_load avioflow-online-load-audio.cpp)
target_link_libraries(avioflow_online_load PRIVATE avioflow)

# Unit tests - Decoder tests
add_executable(avioflow_load_test ffmpeg/decoder/load-test.cpp)
target_include_directories(avioflow_load_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(avioflow_load_test PRIVATE avioflow)

add_executable(avioflow_resample_test ffmpeg/decoder/resample-test.cpp)
target_include_directories(avioflow_resample_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(avioflow_resample_test PRIVATE avioflow)

# Copy FFmpeg DLLs and avioflow.dll to test executables - Windows Shared only
if(BUILD_SHARED_LIBS AND WIN32)
    set(TEST_TARGETS 
        avioflow_offline_load 
        avioflow_online_load 
        avioflow_load_test 
        avioflow_resample_test
    )
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
