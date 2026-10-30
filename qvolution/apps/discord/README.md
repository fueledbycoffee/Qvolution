# Discord

Status: **not tested**. Wave 4; priority 20.

Source: https://discord.com/download

Use the unmodified official Mac client. Passing login is not working Discord; capture APIs are currently incomplete.

## Acceptance workflow

1. Render login and authenticate interactively.
2. Navigate servers and exchange an authorized test message.
3. Test voice, then camera, then screen share as separate gates.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run discord`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
