# Tests CMakeLists.txt

# Unit tests - Decoder tests
add_executable(ffmpeg-decoder-offline-load-test ffmpeg/decoder-offline-load-test.cpp)
target_include_directories(ffmpeg-decoder-offline-load-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-decoder-offline-load-test PRIVATE avioflow)

add_executable(ffmpeg-decoder-online-load-test ffmpeg/decoder-online-load-test.cpp)
target_include_directories(ffmpeg-decoder-online-load-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-decoder-online-load-test PRIVATE avioflow)

add_executable(ffmpeg-decoder-resample-test ffmpeg/decoder-resample-test.cpp)
target_include_directories(ffmpeg-decoder-resample-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-decoder-resample-test PRIVATE avioflow)

add_executable(ffmpeg-device-list-test ffmpeg/device-list-test.cpp)
target_include_directories(ffmpeg-device-list-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffmpeg-device-list-test PRIVATE avioflow)

add_executable(ffmpeg-info-test ffmpeg/ffmpeg-info-test.cpp)
target_link_libraries(ffmpeg-info-test PRIVATE avioflow)

