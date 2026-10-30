# CotEditor

Status: **not tested**. Wave 2; priority 7.

Source: https://coteditor.com/

Modern Apple framework coverage needs validation.

## Acceptance workflow

1. Open and edit a UTF-8 source file.
2. Find, replace, undo and save.
3. Reopen and compare exact contents.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run coteditor`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
