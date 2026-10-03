add_subdirectory(layouts)

file(GLOB_RECURSE DARKEYE_UI_HEADERS CONFIGURE_DEPENDS
    "pages/*.h"
    "dialogs/*.h"
    "components/*.h"
)
file(GLOB_RECURSE DARKEYE_UI_SOURCES CONFIGURE_DEPENDS
    "pages/*.cpp"
    "dialogs/*.cpp"
    "components/*.cpp"
)
qt_add_library(darkeye_business_ui STATIC
    ${DARKEYE_UI_HEADERS}
    ${DARKEYE_UI_SOURCES}
)

set(_darkeye_shelf_qml_dir "${PROJECT_SOURCE_DIR}/resources/qml/dvd")
set(_darkeye_shelf_qml_files
    "${_darkeye_shelf_qml_dir}/dvd_scene.qml"
    "${_darkeye_shelf_qml_dir}/Dvd.qml"
)
set(_darkeye_shelf_qml_resources
    "${_darkeye_shelf_qml_dir}/icons/love-off.svg"
    "${_darkeye_shelf_qml_dir}/icons/love-on.svg"
    "${_darkeye_shelf_qml_dir}/icons/square-pen.svg"
    "${_darkeye_shelf_qml_dir}/icons/trash-2.svg"
)
set_source_files_properties("${_darkeye_shelf_qml_dir}/dvd_scene.qml"
    PROPERTIES QT_RESOURCE_ALIAS "dvd_scene.qml")
set_source_files_properties("${_darkeye_shelf_qml_dir}/Dvd.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Dvd.qml")
foreach(_darkeye_shelf_icon IN LISTS _darkeye_shelf_qml_resources)
    get_filename_component(_darkeye_shelf_icon_name "${_darkeye_shelf_icon}" NAME)
    set_source_files_properties("${_darkeye_shelf_icon}"
        PROPERTIES QT_RESOURCE_ALIAS "icons/${_darkeye_shelf_icon_name}")
endforeach()
qt_add_qml_module(darkeye_business_ui
    URI Darkeye.Shelf
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Darkeye/Shelf"
    NO_PLUGIN
    QML_FILES ${_darkeye_shelf_qml_files}
    RESOURCES ${_darkeye_shelf_qml_resources}
)
unset(_darkeye_shelf_icon)
unset(_darkeye_shelf_icon_name)
unset(_darkeye_shelf_qml_resources)
unset(_darkeye_shelf_qml_files)
unset(_darkeye_shelf_qml_dir)

set_property(TARGET darkeye_business_ui PROPERTY AUTOMOC ON)
add_library(darkeye::ui ALIAS darkeye_business_ui)
target_include_directories(darkeye_business_ui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
)
target_link_libraries(darkeye_business_ui PUBLIC
    darkeye::core
    darkeye::settings
    darkeye::database
    darkeye::services
    darkeye::graph_view
    darkeye::darkeye_ui
    darkeye::docking
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
    Qt6::QuickWidgets
    Qt6::Widgets
)
target_compile_definitions(darkeye_business_ui PRIVATE
    DARKEYE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
darkeye_apply_compiler_options(darkeye_business_ui)
