# Firefox

Status: **blocked during XPCOM loading, stage 22**. Wave 3; priority 16.

Source: https://www.mozilla.org/firefox/

Separate browser/multiprocess target; keep sandbox limitations explicit.

Official unmodified Firefox 159 universal remains unthinned. Stage 22 clears the custom-action class after its genuine copy/weak/callback model tests pass, then XPCOM stops at AVFoundation AVSpeechSynthesisVoice (exit 255). All 110 bundle files are unchanged and exact-prefix cleanup returns 0. No browser UI, workflow, sandbox, GPU or stability qualification. See [stage22 evidence](../../verification/stage22-fonts-symbols-actions.json).

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
