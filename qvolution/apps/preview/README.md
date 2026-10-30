# Preview

Status: **not tested**. Wave 1; priority 5.

Source: https://support.apple.com/guide/preview/welcome/mac

Use test documents; record fidelity and rendering failures.

## Acceptance workflow

1. Open a local PNG and a multipage PDF.
2. Zoom, navigate and select text.
3. Annotate a disposable PDF, save and reopen.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run preview`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
