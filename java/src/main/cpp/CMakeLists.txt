if(DEFINED ENV{JAVA_HOME})
    set(AVIOFLOW_JAVA_HOME "$ENV{JAVA_HOME}")
else()
    find_program(AVIOFLOW_JAVAC javac REQUIRED)
    get_filename_component(AVIOFLOW_JAVAC_REAL "${AVIOFLOW_JAVAC}" REALPATH)
    get_filename_component(AVIOFLOW_JAVA_BIN "${AVIOFLOW_JAVAC_REAL}" DIRECTORY)
    get_filename_component(AVIOFLOW_JAVA_HOME "${AVIOFLOW_JAVA_BIN}" DIRECTORY)
endif()

set(AVIOFLOW_JNI_PLATFORM_INCLUDE "")
if(WIN32)
    set(AVIOFLOW_JNI_PLATFORM_INCLUDE "${AVIOFLOW_JAVA_HOME}/include/win32")
elseif(APPLE)
    set(AVIOFLOW_JNI_PLATFORM_INCLUDE "${AVIOFLOW_JAVA_HOME}/include/darwin")
else()
    set(AVIOFLOW_JNI_PLATFORM_INCLUDE "${AVIOFLOW_JAVA_HOME}/include/linux")
endif()

add_library(avioflow_jni SHARED avioflow_jni.cpp)

target_include_directories(avioflow_jni PRIVATE
    "${AVIOFLOW_JAVA_HOME}/include"
    "${AVIOFLOW_JNI_PLATFORM_INCLUDE}"
    ${CMAKE_SOURCE_DIR}/avioflow/include
)

target_link_libraries(avioflow_jni PRIVATE avioflow)

set_target_properties(avioflow_jni PROPERTIES
    OUTPUT_NAME "avioflow_jni"
)
