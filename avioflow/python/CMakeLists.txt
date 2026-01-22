include(../../cmake/pybind11.cmake)

# Ensure pybind11 macros are available locally
# find_package(pybind11 REQUIRED) or adding tools to module path is usually needed
# Since FetchContent was used, we need to point to the tools directory
FetchContent_GetProperties(pybind11)
list(APPEND CMAKE_MODULE_PATH "${pybind11_SOURCE_DIR}/tools")
include(pybind11Tools)

set(PY_TARGET_NAME _avioflow)
pybind11_add_module(${PY_TARGET_NAME} bindings.cpp)

# Header include paths
target_include_directories(${PY_TARGET_NAME} PRIVATE ../include)

# Link main target
target_link_libraries(${PY_TARGET_NAME} PRIVATE avioflow)

# --- Integrated Package Generation ---
# The package will be created in the bin/Release/avioflow directory
set(PACKAGE_OUTPUT_DIR "$<TARGET_FILE_DIR:${PY_TARGET_NAME}>/avioflow")

add_custom_command(TARGET ${PY_TARGET_NAME} POST_BUILD
    # Ensure package directory exists
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_OUTPUT_DIR}"
    
    # Copy the compiled extension (.pyd/.so) into the package
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PY_TARGET_NAME}>" "${PACKAGE_OUTPUT_DIR}/"
    
    # Copy the __init__.py from our source tree
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/__init__.py" "${PACKAGE_OUTPUT_DIR}/"
    
    # Copy main library (avioflow.dll)
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:avioflow>" "${PACKAGE_OUTPUT_DIR}/"
    
    # Copy all FFmpeg dependencies
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${FFMPEG_BIN_DIR}" "${PACKAGE_OUTPUT_DIR}"
    
    COMMENT "Building avioflow Python package at ${PACKAGE_OUTPUT_DIR}"
)
