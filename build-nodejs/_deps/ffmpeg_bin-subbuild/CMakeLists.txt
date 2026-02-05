# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.22.1)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(ffmpeg_bin-populate NONE)



include(ExternalProject)
ExternalProject_Add(ffmpeg_bin-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "/data/user/lxp/docker/avioflow/public/downloads/ffmpeg-7.1-static-x86_64-linux-gnu.tar.gz" "DOWNLOAD_NO_PROGRESS" "FALSE"
                    SOURCE_DIR          "/data/user/lxp/docker/avioflow/build-nodejs/_deps/ffmpeg_bin-src"
                    BINARY_DIR          "/data/user/lxp/docker/avioflow/build-nodejs/_deps/ffmpeg_bin-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


