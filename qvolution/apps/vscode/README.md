# Visual Studio Code

Status: **not tested**. Wave 3; priority 14.

Source: https://code.visualstudio.com/

Mac Electron candidate after an independent minimal Electron fixture passes.

## Acceptance workflow

1. Open a fixture folder and edit a file.
2. Use search and save/reopen.
3. Open the integrated terminal and verify output.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run vscode`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
