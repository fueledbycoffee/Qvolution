# XPC client-test build correction

The ENABLE_TESTS build failed compiling `libxpc_test_nonexistent_service`: it inherited `__XPC_BUILDING_XPC__` from the library directory. That macro prevents xpc.h from exposing connection.h, so the fixture could not see `XPC_ERROR_CONNECTION_INVALID` or connection function declarations.

Apply `-U__XPC_BUILDING_XPC__` only to that client-test target in the superproject. The library and other internal test compilation are unchanged. The source of the contract is the pinned libxpc CMake definitions and public-header conditional includes already in the tree; no binary inspection is involved.

The original compile command fails with the undeclared error identifier. Replaying that production command with the final undefine compiles successfully, preserving the fixture instead of disabling ENABLE_TESTS. The normal configured Ninja target is checked separately in the build log. Compilation is not a passing guest XPC runtime test.
