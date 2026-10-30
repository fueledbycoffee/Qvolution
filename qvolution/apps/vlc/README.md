# VLC

Status: **not tested**. Wave 2; priority 11.

Source: https://www.videolan.org/vlc/

Measure A/V behavior, not just a visible player window.

## Acceptance workflow

1. Play a local sample with audio.
2. Seek, pause and toggle fullscreen.
3. Open a second media file and exit cleanly.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run vlc`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
