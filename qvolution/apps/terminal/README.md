# Terminal

Status: **not tested**. Wave 1; priority 2.

Source: https://support.apple.com/guide/terminal/welcome/mac

Requires an acquired Mac application bundle; a shell prompt alone does not prove PTY correctness.

## Acceptance workflow

1. Open a shell and run pwd and printf.
2. Test PTY input, Unicode and resize.
3. Open a second window and exit cleanly.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run terminal`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.

## Runtime prerequisite

The source-built `qvolution_pty_contract` probe covers controlling PTYs, foreground
process groups, UTF-8 I/O, resize/SIGWINCH, session isolation and exit/reaping.
Positive warm/short-cold runs and the deliberately missing-resize failure control
have been observed. See `qvolution/probes/pty-contract.md` for the initial timeout
and long-prefix confound; these results do not qualify Terminal.app.

There is no Terminal GUI source fixture in the pinned checkout. Acquire an
unmodified Terminal.app legitimately and pin its source macOS release, app version,
architecture and executable hash. Do not substitute another terminal or download
Apple stock binaries from an unofficial archive.
