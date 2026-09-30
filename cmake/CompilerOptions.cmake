set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

function(darkeye_apply_compiler_options target)
    target_compile_features(${target} PUBLIC cxx_std_20)

    if(MSVC)
        # Qt's imported targets already expose their include directories through
        # /external:I and lower normal compiler diagnostics with /external:W0.
        # MSVC code analysis is controlled separately, so keep it out of all
        # third-party headers as well.  This avoids C26495/C26813/C26478 reports
        # originating in Qt while preserving analysis for Darkeye sources.
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
            /analyze:external-
        )
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
