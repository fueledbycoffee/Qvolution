# QV-004: Wayland socket paths

Component commit: `ff38ae04c9eb66c3020b96aa5f49d541ba30bb14` in [Qvolution-cocotron](https://github.com/fueledbycoffee/Qvolution-cocotron/commit/ff38ae04c9eb66c3020b96aa5f49d541ba30bb14).

The selector prepended XDG_RUNTIME_DIR even when WAYLAND_DISPLAY was absolute. Preserve absolute paths, reject truncated relative path construction, and retain explicit backend overrides. Behavior follows the [public Wayland client API](https://wayland.freedesktop.org/docs/html/apb.html).

`python3 tests/wayland-backend-selection.py` in the component checkout fails two absolute-path cases on the parent and passes all nine cases with the patch. The test compiles the production selector, suppresses only NSLog and uses isolated Unix sockets; getenv/access/path construction stay real. The patched source also compiled with both AppKit and CoreGraphics Wayland recipes from the configured x86_64 build, writing independent outputs.

This establishes selection behavior and focused compilation. It does not establish a successful guest application launch or GNOME integration. Qvolution's host runner and CI now include the regression, and the superproject uses the approved component fork with an exact pin.
