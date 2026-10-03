file(GLOB_RECURSE DARKEYE_DATABASE_HEADERS CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE DARKEYE_DATABASE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

qt_add_library(darkeye_database STATIC
    ${DARKEYE_DATABASE_HEADERS}
    ${DARKEYE_DATABASE_SOURCES}
)
add_library(darkeye::database ALIAS darkeye_database)
target_include_directories(darkeye_database PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(darkeye_database PUBLIC darkeye::core darkeye::settings Qt6::Network Qt6::Sql)
target_link_libraries(darkeye_database PRIVATE Qt6::CorePrivate)
if(WIN32)
    target_link_libraries(darkeye_database PRIVATE Advapi32)
elseif(APPLE)
    find_library(DARKEYE_SECURITY_FRAMEWORK Security REQUIRED)
    find_library(DARKEYE_COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(darkeye_database PRIVATE
        ${DARKEYE_SECURITY_FRAMEWORK} ${DARKEYE_COREFOUNDATION_FRAMEWORK})
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
    target_link_libraries(darkeye_database PRIVATE PkgConfig::LIBSECRET)
    target_compile_definitions(darkeye_database PRIVATE DARKEYE_USE_LIBSECRET)
endif()
darkeye_apply_compiler_options(darkeye_database)
