# Transmission

Status: **not tested**. Wave 2; priority 8.

Source: https://transmissionbt.com/

Use a controlled transfer fixture.

## Acceptance workflow

1. Open a local test torrent.
2. Transfer a self-authored fixture on an isolated peer setup.
3. Pause, resume and verify downloaded bytes.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run transmission`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
