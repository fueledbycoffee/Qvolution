# Dia

Status: **not tested**. Wave 4; priority 19.

Source: https://www.diabrowser.com/

Assumed to mean The Browser Company Dia. Catalog requires ARM64 and macOS 14+. Not directly runnable as x86_64 code.

## Acceptance workflow

1. Launch on a matching ARM64 runtime/host.
2. Open tabs and render local/HTTPS pages.
3. Authenticate interactively and test an authorized assistant action.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run dia`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
