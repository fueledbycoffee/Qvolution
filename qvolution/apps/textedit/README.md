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

The title-bar correction is now verified: guest layout checks pass on Wayland and X11, and the user supplied an after screenshot and confirmed one title bar with working typing. The source fixture remains a diagnostic launch because the Swift loader override is still required. Save/reopen subsequently failed: expanding `/tmp` produced flickering, an unresponsive dialog and runtime termination. The directory data source reread and sorted the full listing for every child row.

The snapshot fix is built and staged. Its 1024-file regression fails on the parent at the first child (two directory reads), then passes on the candidate, including refresh, nested-directory interleaving, stable URL identity and invalid bounds. All 13 host steps and three CLI boot/shutdown cycles pass. Actual save/reopen, flickering and runtime survival remain unverified on the candidate; normal Swift loading remains blocked. See `qvolution/verification/save-browser-snapshots.json`.

The snapshot-only retry also failed: the user reported freezing/closure and supplied a GNOME unresponsive-dialog screenshot. A real save-panel nib diagnostic reproduced a timeout during automatic column sizing with about 55,000 entries. The follow-up disables eager column sizing in the panel and sorts filename strings before creating URLs. It expands and displays the real `/tmp` browser (56,193 rows) in 2.973 seconds, and exits/shuts down successfully in 5.26 seconds. Snapshot refresh/identity, all 13 host steps and three CLI cycles pass. Interactive modal Save and save/reopen remain pending. See `qvolution/verification/save-browser-expansion.json`.

The user subsequently confirmed saving succeeds, with flicker/lag remaining on every menu change and while scrolling. `/tmp/Qvolution-test.txt` was verified as 19 bytes containing exactly `Qvolution save test`. The user later confirmed the saved text is visible after reopening.

A separate rendering regression found that `isFlushWindowDisabled` returned an unused flag instead of the active nesting counter and the Wayland pending-presentation gate ignored drawing pauses. The corrected runtime passes nested-pause/resume checks; the parent fails both state and premature-pixel-access checks. The real `/tmp` panel diagnostic, 13 host steps and three CLI cycles still pass. The user confirmed the reopened text is visible, flickering is gone, and the app seems much more responsive. This confirms the diagnostic ASCII save/quit/reopen and visual menu/scroll milestone; Unicode, undo, clipboard, extended stability and normal Swift startup remain outside this result. See `qvolution/verification/presentation-atomicity.json`.
