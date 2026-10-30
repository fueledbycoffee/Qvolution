# Calculator

Status: **not tested**. Wave 1; priority 4.

Source: https://support.apple.com/guide/calculator/welcome/mac

Framework requirements depend strongly on the chosen macOS release.

## Acceptance workflow

1. Compute known arithmetic results.
2. Use keyboard input and mode/menu changes.
3. Copy a result to TextEdit.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run calculator`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
