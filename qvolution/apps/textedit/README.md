# TextEdit

Status: **source fixture built; normal loading blocked**. Wave 1; priority 1.

Source: https://support.apple.com/guide/textedit/welcome/mac

Darling-built open-source TextEdit is the first fixture; test an unmodified Apple-distributed build separately.

## Acceptance workflow

1. Create a Unicode document.
2. Edit, undo, copy and paste.
3. Save, quit, reopen and compare contents.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run textedit`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.

## First runtime checkpoint

The source fixture at `b85f62f309a4b1bc655d056df4d4a72f9083cbe2` builds and stages. Normal loading fails because libswiftFoundation requests `__swift_classIsSwiftMask` from libswiftCore. The repository supplies the symbol in a separate compatibility library, but it does not satisfy this normal two-level bind.

A diagnostic launch with `DYLD_FORCE_FLAT_NAMESPACE=1` and `DYLD_INSERT_LIBRARIES=/usr/lib/swift/libswiftCoreCompat.dylib` gets past that loader failure. Wayland connects, and the user supplied a screenshot showing rendered text and confirmed that the window looks broken. It has duplicate title bars from WaylandWindow and NSThemeFrame. Record this as diagnostic progress, not working app compatibility. Save/reopen and normal loading have not passed.
