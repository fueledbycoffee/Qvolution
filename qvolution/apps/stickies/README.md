# Stickies

Status: **not tested**. Wave 1; priority 3.

Source: https://support.apple.com/guide/stickies/welcome/mac

Acquire an exact Mac application version.

## Acceptance workflow

1. Create and edit two notes.
2. Copy and paste between windows.
3. Quit and verify note persistence after relaunch.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run stickies`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
