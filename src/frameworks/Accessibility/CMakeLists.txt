project(AccessibilityFramework)
remove_sdk_framework(Accessibility)
set(DYLIB_COMPAT_VERSION "1.0.0")
set(DYLIB_CURRENT_VERSION "1.0.0")
generate_sdk_framework(Accessibility VERSION A HEADER "include/Accessibility")
# The private /usr/lib/libAccessibility.dylib already owns the target name.
add_framework(Accessibility FAT CURRENT_VERSION VERSION A
    TARGET_NAME AccessibilityFramework
    SOURCES src/AXCustomContent.m
    DEPENDENCIES system objc Foundation)
# REGENERATE_SDK is off by default, so new headers need a local search path.
target_include_directories(AccessibilityFramework PRIVATE include)

if (ENABLE_TESTS)
    add_darling_executable(accessibility_import_exports tests/import-exports.c)
    add_darling_executable(accessibility_import_behavior tests/import-behavior.m)
    target_link_libraries(accessibility_import_behavior Foundation objc)
    target_include_directories(accessibility_import_behavior PRIVATE include)
endif()
