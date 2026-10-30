# Obsidian

Status: **not tested**. Wave 3; priority 15.

Source: https://obsidian.md/

Mac Electron candidate; disable optional community plugins in baseline tests.

## Acceptance workflow

1. Create a disposable local vault.
2. Edit and link two notes.
3. Restart and verify note contents.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run obsidian`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
