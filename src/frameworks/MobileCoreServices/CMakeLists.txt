project(MobileCoreServices)

# The deprecated MobileCoreServices UTI C API is implemented by LaunchServices.
# Keep a distinct Catalyst install name and reexport that implementation so
# clients bind to the real functions and constants rather than duplicate stubs.
set(DYLIB_COMPAT_VERSION "1.0.0")
set(DYLIB_CURRENT_VERSION "1.0.0")
set(FRAMEWORK_VERSION "A")

add_framework(MobileCoreServices
    IOSSUPPORT
    FAT
    CURRENT_VERSION
    VERSION ${FRAMEWORK_VERSION}
    SOURCES anchor.c
    DEPENDENCIES LaunchServices system
)

set(LaunchServices_BUILD ${CMAKE_BINARY_DIR}/src/frameworks/CoreServices/src/LaunchServices/LaunchServices)
reexport(MobileCoreServices LaunchServices ${LaunchServices_BUILD})
