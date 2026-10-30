# Wayland title-bar ownership

Pin the UI component at `1b0108b95951079fae6e26e2bf8dacb6f4c7fdce`. Wayland already supplies client or server decorations; the AppKit theme must not reserve and draw a second title bar. Existing backends default to the previous theme policy; Wayland reports that it provides the title bar.

The guest fixture is built as `qvolution_titlebar_policy` with ENABLE_TESTS and QVOLUTION_BUILD_TEXTEDIT. Expected title reservation: zero on Wayland, 28 on the unchanged X11 theme path; borderless reservation stays zero and frame/content conversion must round-trip.

The old Wayland runtime demonstrably fails at 28 rather than zero. The candidate's three changed implementation files compile with production recipes and candidate headers. Integrated after-fix checks now pass: expected height zero on Wayland and 28 on X11, zero borderless reservation and frame/content round trips. Both dedicated fixture prefixes shut down successfully. The user supplied an after screenshot and confirmed a single title bar with working typing.

The fixture currently needs the same explicit Swift compatibility diagnostic as source TextEdit. Neither a layout pass nor a screenshot establishes normal Swift loading or edit/save/reopen compatibility.
