include(../../cmake/pybind11.cmake)
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)

FetchContent_GetProperties(pybind11)
include(${pybind11_SOURCE_DIR}/tools/pybind11Tools.cmake)

# Define the package name
set(PY_PACKAGE_NAME avioflow)
set(PY_EXT_NAME _avioflow)

# Build the extension
pybind11_add_module(${PY_EXT_NAME} bindings.cpp)

# Header include paths
target_include_directories(${PY_EXT_NAME} PRIVATE ../include)

# Link main target
target_link_libraries(${PY_EXT_NAME} PRIVATE avioflow)

# --- Automatic Package Structure in Build Directory ---
# This avoids manual directory creation and makes 'import avioflow' work if build/bin is in PYTHONPATH

# Set output directory to bin/avioflow
set_target_properties(${PY_EXT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${PY_PACKAGE_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${PY_PACKAGE_NAME}"
)

# Copy __init__.py to the same directory
add_custom_command(TARGET ${PY_EXT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/${PY_PACKAGE_NAME}/__init__.py"
        "$<TARGET_FILE_DIR:${PY_EXT_NAME}>/__init__.py"
    COMMENT "Copying __init__.py to package directory"
)

# --- Installation ---

# Determine Python site-packages directory for "global" installation
execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import site; print(site.getsitepackages()[0])"
    OUTPUT_VARIABLE PYTHON_SITE_PACKAGES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Option to install to site-packages or just use the default CMAKE_INSTALL_PREFIX
option(INSTALL_PYTHON_PACKAGE_TO_SITE_PACKAGES "Install the Python package directly to site-packages" OFF)

if(INSTALL_PYTHON_PACKAGE_TO_SITE_PACKAGES)
    set(PYTHON_INSTALL_DESTINATION "${PYTHON_SITE_PACKAGES}")
else()
    set(PYTHON_INSTALL_DESTINATION "lib/python/site-packages") # Default relative path
endif()

message(STATUS "Python package will be installed to: ${PYTHON_INSTALL_DESTINATION}")

# Install the package
install(TARGETS ${PY_EXT_NAME}
    DESTINATION "${PYTHON_INSTALL_DESTINATION}/${PY_PACKAGE_NAME}"
)

# Also install the main library so auditwheel/delocate can find it for bundling
install(TARGETS avioflow
    RUNTIME DESTINATION "${PYTHON_INSTALL_DESTINATION}/${PY_PACKAGE_NAME}"
    LIBRARY DESTINATION "${PYTHON_INSTALL_DESTINATION}/${PY_PACKAGE_NAME}"
)

# Install FFmpeg shared libraries into the package directory so they are included in the wheel
if(UNIX AND NOT APPLE)
    # On Linux, FFmpeg libs are in the lib directory
    file(GLOB FFMPEG_SOS "${FFMPEG_ROOT}/lib/lib*.so*")
    if(FFMPEG_SOS)
        install(FILES ${FFMPEG_SOS} DESTINATION "${PYTHON_INSTALL_DESTINATION}/${PY_PACKAGE_NAME}")
    endif()
elseif(APPLE)
    # On macOS, FFmpeg libs are in the lib directory as .dylib
    file(GLOB FFMPEG_DYLIBS "${FFMPEG_ROOT}/lib/*.dylib")
    if(FFMPEG_DYLIBS)
        install(FILES ${FFMPEG_DYLIBS} DESTINATION "${PYTHON_INSTALL_DESTINATION}/${PY_PACKAGE_NAME}")
    endif()
elseif(WIN32)
    # On Windows, FFmpeg DLLs are in the bin directory
    file(GLOB FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
    if(FFMPEG_DLLS)
        install(FILES ${FFMPEG_DLLS} DESTINATION "${PYTHON_INSTALL_DESTINATION}/${PY_PACKAGE_NAME}")
    endif()
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${PY_PACKAGE_NAME}/__init__.py"
    DESTINATION "${PYTHON_INSTALL_DESTINATION}/${PY_PACKAGE_NAME}"
)
