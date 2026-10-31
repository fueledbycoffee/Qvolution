if (TARGET OpenGL)
    add_darling_executable(qvolution_gpu_opengl gpu-opengl.m)
    target_link_libraries(qvolution_gpu_opengl AppKit OpenGL)
endif ()
if (TARGET Metal)
    add_darling_executable(qvolution_gpu_metal gpu-metal.m)
    target_link_libraries(qvolution_gpu_metal Foundation)
    add_dependencies(qvolution_gpu_metal Metal)
endif ()

add_darling_executable(qvolution_pty_contract pty-contract.c)

if (TARGET AppKit)
    foreach(fixture IN ITEMS font-selection underline-typing alignment-caret inspector-bar inspector-window-focus)
        string(REPLACE "-" "_" target_suffix "${fixture}")
        add_darling_executable(qvolution_text_${target_suffix}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-text-${fixture}.m)
        target_link_libraries(qvolution_text_${target_suffix} AppKit)
    endforeach()
    add_darling_executable(qvolution_popup_viewport
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-popup-viewport.m)
    target_include_directories(qvolution_popup_viewport PRIVATE ${CMAKE_SOURCE_DIR}/src/external/cocotron)
    target_compile_options(qvolution_popup_viewport PRIVATE -UNDEBUG)
    target_link_libraries(qvolution_popup_viewport AppKit)
    foreach(fixture IN ITEMS parent-output configured-viewport hit-throughput)
        string(REPLACE "-" "_" target_suffix "${fixture}")
        add_darling_executable(qvolution_popup_${target_suffix}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-popup-${fixture}.m)
        target_include_directories(qvolution_popup_${target_suffix} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/external/cocotron
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/CoreGraphics/include
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/AppKit/X11.backend)
        target_compile_options(qvolution_popup_${target_suffix} PRIVATE -UNDEBUG)
        target_link_libraries(qvolution_popup_${target_suffix} AppKit)
    endforeach()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(QVOLUTION_FREETYPE REQUIRED freetype2 fontconfig)
    add_darling_executable(qvolution_freetype_advances
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/freetype-unscaled-advances.m)
    target_include_directories(qvolution_freetype_advances PRIVATE
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/Onyx2D/include
        ${QVOLUTION_FREETYPE_INCLUDE_DIRS})
    target_link_libraries(qvolution_freetype_advances Foundation Onyx2D FreeType)
    add_darling_executable(qvolution_font_fractional_origin
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/coregraphics-font-fractional-origin.m)
    target_include_directories(qvolution_font_fractional_origin PRIVATE
        ${QVOLUTION_FREETYPE_INCLUDE_DIRS})
    target_link_libraries(qvolution_font_fractional_origin Foundation CoreGraphics FreeType)
    pkg_check_modules(QVOLUTION_DBUS dbus-1)
    if (QVOLUTION_DBUS_FOUND)
        add_darling_executable(qvolution_orbit_registration
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-orbit-wayland-registration.m)
        target_include_directories(qvolution_orbit_registration PRIVATE ${QVOLUTION_DBUS_INCLUDE_DIRS})
        target_link_libraries(qvolution_orbit_registration AppKit dbus)
    endif()
endif()
