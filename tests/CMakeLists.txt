# Tests CMakeLists.txt

# Unit tests - Decoder tests
add_executable(ffmpeg-decoder-load-test ffmpeg/decoder-load-test.cpp)
target_include_directories(ffmpeg-decoder-load-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-decoder-load-test PRIVATE avioflow)

add_executable(ffmpeg-decoder-resample-test ffmpeg/decoder-resample-test.cpp)
target_include_directories(ffmpeg-decoder-resample-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-decoder-resample-test PRIVATE avioflow)

add_executable(ffmpeg-device-list-test ffmpeg/device-list-test.cpp)
target_include_directories(ffmpeg-device-list-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-device-list-test PRIVATE avioflow)
