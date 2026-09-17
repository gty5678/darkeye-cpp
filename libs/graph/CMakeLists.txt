file(GLOB_RECURSE DARKEYE_GRAPH_HEADERS CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE DARKEYE_GRAPH_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

qt_add_library(darkeye_graph STATIC
    ${DARKEYE_GRAPH_HEADERS}
    ${DARKEYE_GRAPH_SOURCES}
)
add_library(darkeye::graph ALIAS darkeye_graph)
target_include_directories(darkeye_graph PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/graph>"
)
qt_add_shaders(darkeye_graph forceview_rhi_shaders
    PREFIX "/forceview/shaders"
    FILES
        shaders/force_rhi_color.vert
        shaders/force_rhi_color.frag
        shaders/force_rhi_text.vert
        shaders/force_rhi_text.frag
)
target_link_libraries(darkeye_graph
    PUBLIC Qt6::Widgets
    PRIVATE
        Qt6::GuiPrivate
        msdfgen::msdfgen-core
        msdfgen::msdfgen-ext
        Freetype::Freetype
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(darkeye_graph PRIVATE OpenMP::OpenMP_CXX)
endif()
darkeye_apply_compiler_options(darkeye_graph)
