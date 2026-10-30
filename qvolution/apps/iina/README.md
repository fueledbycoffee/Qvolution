# IINA

Status: **not tested**. Wave 2; priority 12.

Source: https://iina.io/

Graphics, media backend and Swift dependencies need testing.

## Acceptance workflow

1. Play a local sample with audio.
2. Seek and render subtitles.
3. Resize, enter fullscreen and exit cleanly.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run iina`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
