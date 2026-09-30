qt_add_library(darkeye_desktop_ui STATIC
    MainWindow.cpp
    MainWindow.h
)
add_library(darkeye::desktop_ui ALIAS darkeye_desktop_ui)
target_include_directories(darkeye_desktop_ui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)
target_link_libraries(darkeye_desktop_ui PUBLIC darkeye::app)
if(WIN32)
    target_link_libraries(darkeye_desktop_ui PRIVATE Psapi)
endif()
darkeye_apply_compiler_options(darkeye_desktop_ui)

qt_add_executable(Darkeye WIN32
    Application.cpp
    Application.h
    main.cpp
)
if(WIN32)
    target_sources(Darkeye PRIVATE "${PROJECT_SOURCE_DIR}/resources/logo.rc")
endif()
target_link_libraries(Darkeye PRIVATE darkeye::desktop_ui)
darkeye_apply_compiler_options(Darkeye)
darkeye_configure_packaging(Darkeye)

