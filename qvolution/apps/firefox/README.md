# Firefox

Status: **not tested**. Wave 3; priority 16.

Source: https://www.mozilla.org/firefox/

Separate browser/multiprocess target; keep sandbox limitations explicit.

## Acceptance workflow

1. Open a local HTML fixture and an HTTPS page.
2. Type, scroll and open a second tab.
3. Download a fixture and verify bytes.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run firefox`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
