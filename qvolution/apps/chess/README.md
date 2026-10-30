# Chess

Status: **not tested**. Wave 2; priority 6.

Source: https://support.apple.com/guide/chess/welcome/mac

A deliberate graphics and framework stretch target; not an early easy win.

## Acceptance workflow

1. Render the board.
2. Play several legal moves.
3. Save and reopen a game.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run chess`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
