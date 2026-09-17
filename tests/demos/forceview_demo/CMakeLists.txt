qt_add_executable(darkeye_forceview_demo WIN32
    main.cpp
)
target_link_libraries(darkeye_forceview_demo PRIVATE darkeye::graph)
darkeye_apply_compiler_options(darkeye_forceview_demo)
darkeye_configure_packaging(darkeye_forceview_demo)
