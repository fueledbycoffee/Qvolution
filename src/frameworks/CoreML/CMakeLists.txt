project(CoreML)
remove_sdk_framework(CoreML)
set(DYLIB_COMPAT_VERSION "1.0.0")
set(DYLIB_CURRENT_VERSION "1.0.0")
set(FRAMEWORK_VERSION "A")
generate_sdk_framework(CoreML VERSION ${FRAMEWORK_VERSION} HEADER "include/CoreML")
add_framework(CoreML
    FAT CURRENT_VERSION VERSION ${FRAMEWORK_VERSION}
    SOURCES src/CoreML.m
    DEPENDENCIES system objc Foundation
)

add_darling_executable(imp-vision-load-exports tests/load-exports.c)
target_link_libraries(imp-vision-load-exports system)
add_darling_executable(imp-vision-coreml-behavior tests/behavior.m)
target_include_directories(imp-vision-coreml-behavior PRIVATE include)
target_link_libraries(imp-vision-coreml-behavior CoreML Foundation objc system)
