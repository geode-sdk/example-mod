cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project($Template VERSION 0.1.0 LANGUAGES C CXX)

set(SOURCE_FILES main.cpp sdk/include/implicitEntry.cpp)
set(BINARY_NAME $Template)
include(sdk/Geode.cmake)
