# TextEdit

Status: **not tested**. Wave 1; priority 1.

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
