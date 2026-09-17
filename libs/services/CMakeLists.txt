file(GLOB_RECURSE DARKEYE_SERVICE_HEADERS CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE DARKEYE_SERVICE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

qt_add_library(darkeye_services STATIC
    ${DARKEYE_SERVICE_HEADERS}
    ${DARKEYE_SERVICE_SOURCES}
)
add_library(darkeye::services ALIAS darkeye_services)
target_include_directories(darkeye_services PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(darkeye_services PUBLIC darkeye::core darkeye::database)
darkeye_apply_compiler_options(darkeye_services)
