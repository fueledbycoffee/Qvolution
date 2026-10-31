# Keka

Status: **partial client rendering, stage 16**. Wave 2; priority 9. No completed workflow or desktop presentation qualification.

Source: https://www.keka.io/

Official unmodified Keka1.6.8 universal bundle; publisher SHA256 verified and retained unthinned. Stage 11 stopped at a distributed-notification transport exception. Stage 16 clears that gate and produces one diagnostic 699×432 CPU client image of settings. The client output contains generic SF-symbol box placeholders and a duplicate title; NSImage currently draws generic symbol placeholders. These are recorded quality gaps, not native artwork fidelity.

The bounded 25-second launch timed out (exit unknown); exact-prefix shutdown returned 0. All 749 extracted regular-file/link entries were rechecked unchanged. Compositor visibility, user interaction, archive/extraction workflow, clean app exit, GPU acceleration and stability remain unverified. See the [sanitized stage16 evidence](../../verification/stage16-keka-client-render.json).

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
