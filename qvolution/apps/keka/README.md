# Keka

Status: **not tested**. Wave 2; priority 9.

Source: https://www.keka.io/

Test the official Mac bundle.

## Acceptance workflow

1. Create an archive of known fixture files.
2. Extract into a fresh directory.
3. Compare file hashes and test a Unicode filename.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run keka`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
