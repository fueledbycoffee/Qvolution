# Source TextEdit fixture

The inherited superproject commented out its TextEdit subdirectory, so a complete runtime build did not produce the application's executable. Add an opt-in `QVOLUTION_BUILD_TEXTEDIT` option, require the AppKit/gui component explicitly, and assign installation to the gui component. Default behavior for upstream-style runtime builds stays unchanged.

The fixture uses the existing source submodule at `b85f62f309a4b1bc655d056df4d4a72f9083cbe2`. This is an open-source fixture, not evidence for a current unmodified Apple vendor bundle.

Verified: configuration with ENABLE_TESTS and QVOLUTION_BUILD_TEXTEDIT enabled succeeds; the TextEdit target compiles and links through 94 incremental build steps. UI launch and document edit/save/reopen are subsequent gates.
