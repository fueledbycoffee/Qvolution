# Terminal

Status: **not tested**. Wave 1; priority 2.

Source: https://support.apple.com/guide/terminal/welcome/mac

Requires an acquired Mac application bundle; a shell prompt alone does not prove PTY correctness.

## Acceptance workflow

1. Open a shell and run pwd and printf.
2. Test PTY input, Unicode and resize.
3. Open a second window and exit cleanly.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run terminal`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
