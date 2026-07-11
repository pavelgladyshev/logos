# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Keep bootloader_components/main available for later, but use ESP-IDF's
# standard second-stage bootloader for the current build.
set(BOOTLOADER_IGNORE_EXTRA_COMPONENT main)

project(logOS)
idf_build_set_property(COMPILE_DEFINITIONS "ESP32" APPEND)
