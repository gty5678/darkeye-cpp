add_subdirectory(demos)

function(darkeye_add_qt_test target source)
    qt_add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE darkeye::app Qt6::Test)
    darkeye_apply_compiler_options(${target})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${DARKEYE_QT_PLUGIN_DIR}/platforms"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${DARKEYE_QT_BIN_DIR}"
    )
endfunction()

qt_add_executable(darkeye_schema_snapshot_dump support/SchemaSnapshotDump.cpp)
target_link_libraries(darkeye_schema_snapshot_dump PRIVATE darkeye::app)
darkeye_apply_compiler_options(darkeye_schema_snapshot_dump)

qt_add_executable(darkeye_fixture_database_builder
    support/FixtureDatabaseBuilder.cpp
)
target_compile_definitions(darkeye_fixture_database_builder PRIVATE
    DARKEYE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_link_libraries(darkeye_fixture_database_builder PRIVATE darkeye::app)
darkeye_apply_compiler_options(darkeye_fixture_database_builder)

darkeye_add_qt_test(darkeye_app_tests unit/AppPathsTest.cpp)
darkeye_add_qt_test(darkeye_ui_smoke_tests integration/MainWindowSmokeTest.cpp)
target_link_libraries(darkeye_ui_smoke_tests PRIVATE darkeye::desktop_ui)
darkeye_add_qt_test(darkeye_theme_tests unit/ThemeServiceTest.cpp)
darkeye_add_qt_test(darkeye_layout_tests unit/LayoutTest.cpp)
darkeye_add_qt_test(darkeye_media_component_tests unit/MediaComponentTest.cpp)
darkeye_add_qt_test(darkeye_component_library_tests unit/ComponentLibraryTest.cpp)
darkeye_add_qt_test(darkeye_text_widget_tests unit/TextWidgetTest.cpp)
darkeye_add_qt_test(darkeye_utils_compatibility_tests unit/UtilsCompatibilityTest.cpp)
darkeye_add_qt_test(darkeye_video_library_service_tests integration/VideoLibraryServiceTest.cpp)

# Deliberately does not link darkeye::app or darkeye::ui. This target protects
# the reusable component library from acquiring business-UI dependencies.
qt_add_executable(darkeye_ui_boundary_tests unit/DarkeyeUiBoundaryTest.cpp)
target_link_libraries(darkeye_ui_boundary_tests PRIVATE darkeye::darkeye_ui Qt6::Test)
darkeye_apply_compiler_options(darkeye_ui_boundary_tests)
add_test(NAME darkeye_ui_boundary_tests COMMAND darkeye_ui_boundary_tests)
set_tests_properties(darkeye_ui_boundary_tests PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${DARKEYE_QT_PLUGIN_DIR}/platforms"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${DARKEYE_QT_BIN_DIR}"
)
darkeye_add_qt_test(darkeye_myads_tests unit/MyAdsTest.cpp)
darkeye_configure_packaging(darkeye_myads_tests)
set_tests_properties(darkeye_myads_tests PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${DARKEYE_QT_PLUGIN_DIR}/platforms"
)
add_test(NAME darkeye_myads_demo_smoke
    COMMAND $<TARGET_FILE:darkeye_myads_demo> --smoke-test)
set_tests_properties(darkeye_myads_demo_smoke PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${DARKEYE_QT_PLUGIN_DIR}/platforms"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${DARKEYE_QT_BIN_DIR}"
)
add_test(NAME darkeye_graph_view_demo_smoke
    COMMAND $<TARGET_FILE:darkeye_graph_view_demo> --smoke-test)
set_tests_properties(darkeye_graph_view_demo_smoke PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${DARKEYE_QT_PLUGIN_DIR}/platforms"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${DARKEYE_QT_BIN_DIR}"
    TIMEOUT 10
)
add_test(NAME darkeye_component_gallery_smoke
    COMMAND $<TARGET_FILE:darkeye_component_gallery> --smoke-test)
set_tests_properties(darkeye_component_gallery_smoke PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${DARKEYE_QT_PLUGIN_DIR}/platforms"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${DARKEYE_QT_BIN_DIR}"
)
darkeye_add_qt_test(darkeye_database_tests unit/SqliteConnectionTest.cpp)
darkeye_add_qt_test(darkeye_schema_tests integration/SchemaManagerTest.cpp)
darkeye_add_qt_test(darkeye_schema_snapshot_tests integration/SchemaSnapshotTest.cpp)
darkeye_add_qt_test(darkeye_database_fixture_tests compatibility/DatabaseFixtureTest.cpp)
darkeye_add_qt_test(darkeye_serial_number_tests compatibility/SerialNumberCompatibilityTest.cpp)
darkeye_add_qt_test(darkeye_database_backup_tests integration/DatabaseBackupServiceTest.cpp)
darkeye_add_qt_test(darkeye_work_repository_tests integration/WorkRepositoryTest.cpp)
darkeye_add_qt_test(darkeye_work_maintenance_tests integration/WorkMaintenanceServiceTest.cpp)
darkeye_add_qt_test(darkeye_image_fetch_tests integration/ImageFetchServiceTest.cpp)
darkeye_add_qt_test(darkeye_collector_client_tests integration/CollectorClientTest.cpp)
darkeye_add_qt_test(darkeye_work_page_tests integration/WorkPageTest.cpp)
darkeye_add_qt_test(darkeye_shelf_page_tests integration/ShelfPageTest.cpp)
darkeye_add_qt_test(darkeye_av_page_tests integration/AvPageTest.cpp)
darkeye_add_qt_test(darkeye_person_page_tests integration/PersonPageTest.cpp)
darkeye_add_qt_test(darkeye_private_repository_tests integration/PrivateRepositoryTest.cpp)
darkeye_add_qt_test(darkeye_personal_record_dialog_tests integration/PersonalRecordDialogTest.cpp)
darkeye_add_qt_test(darkeye_personal_record_management_tests integration/PersonalRecordManagementWidgetTest.cpp)
darkeye_add_qt_test(darkeye_statistics_repository_tests integration/StatisticsRepositoryTest.cpp)
darkeye_add_qt_test(darkeye_public_repository_tests integration/PublicRepositoryTest.cpp)

qt_add_executable(darkeye_graph_tests
    ../libs/graph/tests/GraphViewSessionTest.cpp
)
target_link_libraries(darkeye_graph_tests PRIVATE darkeye::graph)
darkeye_apply_compiler_options(darkeye_graph_tests)
add_test(NAME darkeye_graph_tests COMMAND darkeye_graph_tests)

qt_add_executable(darkeye_graph_view_physics_tests
    ../libs/graph_view/tests/physics_tests.cpp
)
target_link_libraries(darkeye_graph_view_physics_tests PRIVATE darkeye::graph_view)
darkeye_apply_compiler_options(darkeye_graph_view_physics_tests)
add_test(NAME darkeye_graph_view_physics_tests COMMAND darkeye_graph_view_physics_tests)

qt_add_executable(darkeye_graph_page_rendering_tests
    ../libs/graph_view/tests/GraphPageRenderingTest.cpp
)
target_link_libraries(darkeye_graph_page_rendering_tests PRIVATE darkeye::ui Qt6::Test)
darkeye_apply_compiler_options(darkeye_graph_page_rendering_tests)
add_test(NAME darkeye_graph_page_rendering_tests COMMAND darkeye_graph_page_rendering_tests)
set_tests_properties(darkeye_graph_page_rendering_tests PROPERTIES
    ENVIRONMENT
        "QT_QPA_PLATFORM=windows;QT_QPA_PLATFORM_PLUGIN_PATH=${DARKEYE_QT_PLUGIN_DIR}/platforms"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${DARKEYE_QT_BIN_DIR}"
    TIMEOUT 10
)

target_compile_definitions(darkeye_schema_snapshot_tests PRIVATE
    DARKEYE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(darkeye_database_fixture_tests PRIVATE
    DARKEYE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
target_compile_definitions(darkeye_serial_number_tests PRIVATE
    DARKEYE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
