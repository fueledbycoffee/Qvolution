# Keka

Status: **partial client rendering, stage 22**. Wave 2; priority 9. No completed workflow or desktop presentation qualification.

Source: https://www.keka.io/

Official unmodified Keka1.6.8 universal bundle; publisher SHA256 verified and retained unthinned. Stage 11 stopped at a distributed-notification transport exception. Stage 16 clears that gate and produces one diagnostic 699×432 CPU client image of settings. The client output contains generic SF-symbol box placeholders and a duplicate title; NSImage currently draws generic symbol placeholders. These are recorded quality gaps, not native artwork fidelity.

The bounded 25-second launch timed out (exit unknown); exact-prefix shutdown returned 0. All 749 extracted regular-file/link entries were rechecked unchanged. Compositor visibility, user interaction, archive/extraction workflow, clean app exit, GPU acceleration and stability remain unverified. See the [sanitized stage16 evidence](../../verification/stage16-keka-client-render.json).

Stage 19 repeats the settings client raster after the font/title regression suite passes. Generic symbol boxes and duplicated title/controls remain. Its 25-second run again times out with unknown exit and exact shutdown 0; all 749 payload members remain unchanged. The titleVisibility API correction is proven only by the original renderer fixture, not as the cause of this app appearance. See [stage19 evidence](../../verification/stage19-fonts-title.json).

Stage 22 passes the eleven original font/model/icon gates and renders supported gear/folder artwork in the client image. Other icons are absent and duplicated title/controls remain. The unchanged 749-member bundle again has a bounded 25s timeout, unknown exit and exact cleanup 0. No desktop/workflow/GPU promotion. See [stage22 evidence](../../verification/stage22-fonts-symbols-actions.json).

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
