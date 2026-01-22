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

# --- Integrated Package Generation (for scikit-build-core / cmake --install) ---

# Install the binary extension
install(TARGETS ${PY_TARGET_NAME}
    DESTINATION avioflow
)

# Install the __init__.py
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/__init__.py"
    DESTINATION avioflow
)

# Optionally install the main library and FFmpeg DLLs alongside the extension
# This is mainly for local dev/testing; for wheels, tools like auditwheel/delocate 
# will bundle these into the package.
if(WIN32)
    install(TARGETS avioflow RUNTIME DESTINATION avioflow)
    file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
    install(FILES ${FFMPEG_DLLS} DESTINATION avioflow)
endif()
