# Implicit install-directory modes

Run `python3 tests/install-default-directory-modes.py OUT` under a host build slot.
The test uses the exact setting from CMakeLists.txt and preserves its fixtures.
Under umask 002 it expects newly implicit directories to change from 0775 to 0755.
An existing 0775 directory deliberately remains 0775: use a fresh install or an
explicitly planned repair before relying on this setting for an old installation.

Provenance: rung 3, the installed CMake variable documentation
(`cmake --help-variable CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS`); rung 1,
the vendored launchd source's group-writable-directory rejection. This setting
controls installation behavior and makes no new Apple API claim.
