# Spotify

Status: **not tested**. Wave 4; priority 17.

Source: https://www.spotify.com/download/mac/

Authentication and media-service behavior are separate gates; do not record account tokens.

## Acceptance workflow

1. Render the login flow.
2. Authenticate interactively when ready.
3. Play, pause, seek and verify sustained audio.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run spotify`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
