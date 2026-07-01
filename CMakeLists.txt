cmake_minimum_required(VERSION 3.16) 
 include($ENV{IDF_PATH}/tools/cmake/project.cmake) 
 project(logosTest2) 
 idf_build_set_property(COMPILE_DEFINITIONS "ESP32" APPEND)
