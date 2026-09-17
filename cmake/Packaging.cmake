include(GNUInstallDirs)

function(darkeye_configure_packaging target)
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
            COMMENT "Deploying the Qt runtime beside ${target}"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()

    install(TARGETS ${target} RUNTIME DESTINATION .)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/" DESTINATION resources)

    if(WIN32 AND DARKEYE_DEPLOY_QT_RUNTIME)
        install(CODE [[
            set(_darkeye_installed_executable
                "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/$<TARGET_FILE_NAME:]]${target}[[>")

            if(CMAKE_INSTALL_CONFIG_NAME STREQUAL "Debug")
                set(_darkeye_windeployqt_mode --debug)
            else()
                set(_darkeye_windeployqt_mode --release)
            endif()

            execute_process(
                COMMAND "]]${DARKEYE_WINDEPLOYQT_EXECUTABLE}[["
                    "${_darkeye_windeployqt_mode}"
                    --no-translations
                    --compiler-runtime
                    "${_darkeye_installed_executable}"
                RESULT_VARIABLE _darkeye_windeployqt_result
            )
            if(NOT _darkeye_windeployqt_result EQUAL 0)
                message(FATAL_ERROR
                    "windeployqt failed for ${_darkeye_installed_executable} "
                    "(exit code: ${_darkeye_windeployqt_result})")
            endif()
        ]])
    endif()
endfunction()
