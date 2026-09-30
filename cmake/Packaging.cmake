include(GNUInstallDirs)

function(darkeye_configure_packaging target)

    # --------------------------------
    # 开发阶段：让 build 目录中的程序可以直接运行
    # --------------------------------
    if(WIN32 AND DARKEYE_DEPLOY_QT_RUNTIME)
        find_program(DARKEYE_WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt windeployqt6
            HINTS "${DARKEYE_QT_BIN_DIR}"
            REQUIRED
        )

        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${DARKEYE_WINDEPLOYQT_EXECUTABLE}"
                "$<$<CONFIG:Debug>:--debug>"
                "$<$<NOT:$<CONFIG:Debug>>:--release>"
                --no-translations
                --dir "$<TARGET_FILE_DIR:${target}>"
                "$<TARGET_FILE:${target}>"
            COMMENT "Deploying Qt runtime beside ${target}"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()

    # --------------------------------
    # install：安装程序本身
    # --------------------------------
    install(
        TARGETS ${target}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Qt's deployment script only installs Qt runtime files. msdfgen is a
    # shared vcpkg dependency, so install both imported DLLs and recursively
    # collect their non-system dependencies (FreeType, PNG, zlib, and so on).
    if(WIN32)
        install(
            IMPORTED_RUNTIME_ARTIFACTS
                msdfgen::msdfgen-core
                msdfgen::msdfgen-ext
            RUNTIME_DEPENDENCY_SET darkeye_msdfgen_runtime_dependencies
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

        install(
            RUNTIME_DEPENDENCY_SET darkeye_msdfgen_runtime_dependencies
            DIRECTORIES
                "$<$<CONFIG:Debug>:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin>"
                "$<$<NOT:$<CONFIG:Debug>>:${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin>"
            PRE_EXCLUDE_REGEXES
                "^api-ms-"
                "^ext-ms-"
            POST_EXCLUDE_REGEXES
                "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    # resources 放在 exe 旁边，这个是无论如何都放着的
    install(
        DIRECTORY "${PROJECT_SOURCE_DIR}/resources/"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/resources"
    )
    # data 放在 exe 旁边
    if(DARKEYE_INSTALL_DATA)
        install(
            DIRECTORY "${PROJECT_SOURCE_DIR}/data/"
            DESTINATION "${CMAKE_INSTALL_BINDIR}/data"
        )
    endif()

    # --------------------------------
    # install：部署 Qt DLL / plugins
    # --------------------------------
    qt_generate_deploy_app_script(
        TARGET ${target}
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
        NO_UNSUPPORTED_PLATFORM_ERROR
    )

    install(SCRIPT "${deploy_script}")

endfunction()
