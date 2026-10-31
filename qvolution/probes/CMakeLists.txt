if (TARGET OpenGL)
    add_darling_executable(qvolution_gpu_opengl gpu-opengl.m)
    target_link_libraries(qvolution_gpu_opengl AppKit OpenGL)
    if (TARGET QuartzCore)
        add_darling_executable(qvolution_gpu_composition
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/quartzcore-gpu-composition.m)
        target_link_libraries(qvolution_gpu_composition AppKit QuartzCore OpenGL)
    endif ()
endif ()
if (TARGET Metal)
    add_darling_executable(qvolution_gpu_metal gpu-metal.m)
    target_link_libraries(qvolution_gpu_metal Foundation)
    add_dependencies(qvolution_gpu_metal Metal)
    add_darling_executable(qvolution_metal_stage_input
        ${CMAKE_SOURCE_DIR}/src/external/metal/tests/metal-stage-input.m)
    target_compile_options(qvolution_metal_stage_input PRIVATE -UNDEBUG)
    target_link_libraries(qvolution_metal_stage_input Foundation Metal)
    if (BUILD_METAL AND TARGET Metal_private)
        add_darling_executable(qvolution_metal_stage_conversion
            ${CMAKE_SOURCE_DIR}/src/external/metal/tests/metal-stage-input-conversion.mm)
        target_compile_options(qvolution_metal_stage_conversion PRIVATE -UNDEBUG)
        target_include_directories(qvolution_metal_stage_conversion PRIVATE
            ${CMAKE_SOURCE_DIR}/src/external/metal/deps/indium/include)
        set_target_properties(qvolution_metal_stage_conversion PROPERTIES
            CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
            OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED ON OBJCXX_EXTENSIONS OFF)
        target_link_libraries(qvolution_metal_stage_conversion Foundation Metal Metal_private cxx)
    endif ()
endif ()

add_darling_executable(qvolution_pty_contract pty-contract.c)
if (TARGET CoreGraphics AND TARGET Foundation)
    foreach(fixture IN ITEMS font-names font-name-vectors type1-font-names)
        string(REPLACE "-" "_" target_suffix "${fixture}")
        add_darling_executable(qvolution_cg_${target_suffix}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/coregraphics-${fixture}.m)
        target_link_libraries(qvolution_cg_${target_suffix} Foundation CoreGraphics)
    endforeach()
endif()
if (TARGET CoreText AND TARGET CoreGraphics AND TARGET Foundation)
    add_darling_executable(qvolution_cg_postscript_roundtrip
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/coregraphics-postscript-roundtrip.m)
    target_link_libraries(qvolution_cg_postscript_roundtrip Foundation CoreText CoreGraphics)
    foreach(fixture IN ITEMS ui-font-roles font-traits-dictionary font-name-attribute
            font-metadata font-descriptor)
        string(REPLACE "-" "_" target_suffix "${fixture}")
        add_darling_executable(qvolution_${target_suffix}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/coretext-${fixture}.m)
        target_link_libraries(qvolution_${target_suffix} Foundation CoreText CoreGraphics)
    endforeach()
endif()
add_darling_executable(qvolution_unix_output_capacity
    ${CMAKE_SOURCE_DIR}/src/external/xnu/darling/tests/qvolution/unix-output-capacity.c)
add_darling_executable(qvolution_unix_root_alias
    ${CMAKE_SOURCE_DIR}/src/external/xnu/darling/tests/qvolution/unix-root-alias-guest.c)

if (TARGET CoreServices AND TARGET Foundation)
    add_darling_executable(qvolution_user_activity_forwarding user-activity-forwarding.m)
    target_link_libraries(qvolution_user_activity_forwarding CoreServices objc)
endif()

if (TARGET CoreMedia)
    add_darling_executable(qvolution_coremedia_sample_attachment_keys coremedia-sample-attachment-keys.c)
    target_link_libraries(qvolution_coremedia_sample_attachment_keys CoreFoundation CoreMedia)
    add_darling_executable(qvolution_coremedia_invalid_timing
        coremedia-invalid-timing.c coremedia-invalid-timing-header.c)
    target_link_libraries(qvolution_coremedia_invalid_timing CoreFoundation CoreMedia)
endif()

if (TARGET LaunchServices)
    add_darling_executable(qvolution_url_handler_preferences url-handler-preferences.m)
    target_include_directories(qvolution_url_handler_preferences PRIVATE
        ${CMAKE_SOURCE_DIR}/src/frameworks/CoreServices/include)
    target_link_libraries(qvolution_url_handler_preferences Foundation LaunchServices)
    add_darling_executable(qvolution_url_handler_locking url-handler-locking.m)
    target_include_directories(qvolution_url_handler_locking PRIVATE
        ${CMAKE_SOURCE_DIR}/src/frameworks/CoreServices/include)
    target_link_libraries(qvolution_url_handler_locking Foundation LaunchServices sqlite3)
endif()

if (TARGET UserNotifications)
    add_darling_executable(qvolution_usernotifications_type_hint_key
        usernotifications-type-hint-key.m usernotifications-type-hint-key-header.m)
    target_link_libraries(qvolution_usernotifications_type_hint_key Foundation)
    add_dependencies(qvolution_usernotifications_type_hint_key UserNotifications)
endif()

if (TARGET AuthenticationServices)
    add_darling_executable(qvolution_authorization_domain authorization-domain.m)
    target_link_libraries(qvolution_authorization_domain Foundation)
    add_dependencies(qvolution_authorization_domain AuthenticationServices)
    add_darling_executable(qvolution_web_auth_domain web-auth-domain.m web-auth-domain-header.m)
    target_link_libraries(qvolution_web_auth_domain Foundation)
    add_dependencies(qvolution_web_auth_domain AuthenticationServices)
endif()
if (TARGET SharedFileList AND TARGET CoreServices)
    add_darling_executable(qvolution_shared_list_resolved_url shared-list-resolved-url.m)
    target_include_directories(qvolution_shared_list_resolved_url PRIVATE
        ${CMAKE_SOURCE_DIR}/src/frameworks/CoreServices/include)
    target_link_libraries(qvolution_shared_list_resolved_url Foundation CoreServices)
endif()

if (TARGET AppKit)
    foreach(fixture IN ITEMS semantic-symbols symbol-rendering font-weight-constants)
        string(REPLACE "-" "_" target_suffix "${fixture}")
        add_darling_executable(qvolution_${target_suffix}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-${fixture}.m)
        target_link_libraries(qvolution_${target_suffix} AppKit CoreGraphics)
    endforeach()
    add_darling_executable(qvolution_window_display_flush
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-window-display-flush.m)
    target_link_libraries(qvolution_window_display_flush AppKit CoreGraphics)
    add_darling_executable(qvolution_accessibility_custom_action
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-accessibility-custom-action.m
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-accessibility-custom-action-header.m)
    target_link_libraries(qvolution_accessibility_custom_action Foundation)
    add_dependencies(qvolution_accessibility_custom_action AppKit)
    add_darling_executable(qvolution_title_visibility
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-title-visibility.m)
    target_link_libraries(qvolution_title_visibility AppKit CoreGraphics)
    if (TARGET CoreText)
        add_darling_executable(qvolution_appkit_font_descriptor
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-coretext-font-descriptor.m)
        target_link_libraries(qvolution_appkit_font_descriptor Foundation CoreText CoreGraphics)
        add_dependencies(qvolution_appkit_font_descriptor AppKit)
        add_darling_executable(qvolution_font_ctfont_bridge
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-font-ctfont-bridge.m)
        target_link_libraries(qvolution_font_ctfont_bridge AppKit CoreText CoreGraphics)
    endif()
    add_darling_executable(qvolution_accessibility_popover_role
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-accessibility-popover-role.m
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-accessibility-popover-role-header.m)
    target_link_libraries(qvolution_accessibility_popover_role Foundation)
    add_dependencies(qvolution_accessibility_popover_role AppKit)
    foreach(fixture IN ITEMS display-online display-online-topology)
        string(REPLACE "-" "_" target_suffix "${fixture}")
        add_darling_executable(qvolution_${target_suffix}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/coregraphics-${fixture}.m)
        target_compile_options(qvolution_${target_suffix} PRIVATE -UNDEBUG)
        target_link_libraries(qvolution_${target_suffix} AppKit CoreGraphics)
    endforeach()
    foreach(fixture IN ITEMS screen-colorspace screen-localized-name screen-name-metadata
            colorspace-icc-profile graphics-context-nil-state
            catalog-alpha placeholder-text-color control-accent-color
            semantic-dark-colors semantic-control-surface textview-semantic-defaults
            effective-appearance-inheritance appearance-observer-lifetime
            appearance-reentrancy appearance-direct-lifetime drawing-appearance
            color-dynamic-provider client-capture-window)
        string(REPLACE "-" "_" target_suffix "${fixture}")
        add_darling_executable(qvolution_${target_suffix}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-${fixture}.m)
        target_compile_options(qvolution_${target_suffix} PRIVATE -UNDEBUG)
        target_include_directories(qvolution_${target_suffix} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/external/cocotron)
        target_link_libraries(qvolution_${target_suffix} AppKit CoreGraphics)
    endforeach()
    add_darling_executable(qvolution_drawing_appearance_auto
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-view-drawing-appearance.m)
    target_link_libraries(qvolution_drawing_appearance_auto AppKit QuartzCore)
    add_darling_executable(qvolution_egl_presentation
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-egl-presentation.m)
    target_compile_options(qvolution_egl_presentation PRIVATE -UNDEBUG)
    target_include_directories(qvolution_egl_presentation PRIVATE
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/Onyx2D/include)
    target_link_libraries(qvolution_egl_presentation AppKit OpenGL Onyx2D)
    add_darling_executable(qvolution_png_alpha
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/imageio-png-alpha.m)
    target_link_libraries(qvolution_png_alpha Foundation ImageIO CoreGraphics)
    add_darling_executable(qvolution_client_capture
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-client-capture.m
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/AppKit/Wayland.backend/WaylandClientCapture.m)
    target_include_directories(qvolution_client_capture PRIVATE
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/AppKit/Wayland.backend
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/Onyx2D/include)
    target_link_libraries(qvolution_client_capture Foundation Onyx2D)
    add_darling_executable(qvolution_qt_public_keys
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-qt-public-keys.m)
    target_link_libraries(qvolution_qt_public_keys AppKit)
    add_darling_executable(qvolution_backing_scale
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-backing-scale-observation.m)
    target_link_libraries(qvolution_backing_scale AppKit)
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
    foreach(fixture IN ITEMS flags throughput)
        add_darling_executable(qvolution_font_phase_${fixture}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/coregraphics-font-phase-${fixture}.m)
        target_include_directories(qvolution_font_phase_${fixture} PRIVATE ${QVOLUTION_FREETYPE_INCLUDE_DIRS})
        target_link_libraries(qvolution_font_phase_${fixture} Foundation CoreGraphics FreeType)
    endforeach()
    add_darling_executable(qvolution_font_phase_cache
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/freetype-font-phase-cache.m)
    target_include_directories(qvolution_font_phase_cache PRIVATE
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/Onyx2D/include ${QVOLUTION_FREETYPE_INCLUDE_DIRS})
    target_link_libraries(qvolution_font_phase_cache Foundation Onyx2D FreeType)
    foreach(fixture IN ITEMS xref-stream invalid-input)
        string(REPLACE "-" "_" target_suffix "${fixture}")
        add_darling_executable(qvolution_pdf_${target_suffix}
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/coregraphics-pdf-${fixture}.m)
        target_include_directories(qvolution_pdf_${target_suffix} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/Onyx2D/include)
        target_compile_options(qvolution_pdf_${target_suffix} PRIVATE -UNDEBUG)
        target_link_libraries(qvolution_pdf_${target_suffix} Foundation CoreGraphics Onyx2D)
    endforeach()
    add_darling_executable(qvolution_pdf_predictors
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/onyx-pdf-predictors.m)
    target_include_directories(qvolution_pdf_predictors PRIVATE
        ${CMAKE_SOURCE_DIR}/src/external/cocotron/Onyx2D/include)
    target_compile_options(qvolution_pdf_predictors PRIVATE -UNDEBUG)
    target_link_libraries(qvolution_pdf_predictors Foundation Onyx2D)
    pkg_check_modules(QVOLUTION_DBUS dbus-1)
    if (QVOLUTION_DBUS_FOUND)
        add_darling_executable(qvolution_orbit_registration
            ${CMAKE_SOURCE_DIR}/src/external/cocotron/tests/appkit-orbit-wayland-registration.m)
        target_include_directories(qvolution_orbit_registration PRIVATE ${QVOLUTION_DBUS_INCLUDE_DIRS})
        target_link_libraries(qvolution_orbit_registration AppKit dbus)
    endif()
endif()
