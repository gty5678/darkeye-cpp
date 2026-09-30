cmake_minimum_required(VERSION 3.25)

project(Darkeye VERSION 1.2.5 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(DARKEYE_BUILD_TESTS "Build Darkeye tests" OFF)
option(DARKEYE_DEPLOY_QT_RUNTIME
       "Deploy Qt runtime beside Darkeye after each Windows build" ON)

option(DARKEYE_INSTALL_DATA
    "Install Darkeye data" OFF)

include(CompilerOptions)
include(Dependencies)
include(Packaging)

add_subdirectory(libs)
add_subdirectory(apps)

if(DARKEYE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
