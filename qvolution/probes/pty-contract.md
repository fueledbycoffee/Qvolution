# Guest PTY prerequisite

Build `qvolution_pty_contract` with `ENABLE_TESTS=ON`, then run it from a dedicated
nonroot staged-runtime prefix through `/Volumes/SystemRoot` plus its absolute host
path. Remove diagnostic `DYLD_*` overrides. Shut down the exact prefix after the
bounded run. `--negative-resize` deliberately omits the resize and must fail.

The source-built fixture exercises two real controlling terminals, foreground
process groups, UTF-8 input/output, resize dimensions and asynchronously observed
SIGWINCH, session isolation, and child exit/reaping. It passes on host and in warm
and short fresh guest prefixes; the omitted-resize control fails as expected.
These are library/API contracts, not a GUI terminal or a Terminal.app pass.

One initial warm-path invocation exceeded 20 seconds and remains unexplained.
Later warm and valid short cold controls pass. Separate long-path controls were
invalid: their socket paths exceeded the native Unix-domain limit and silently
truncated. Those controls motivated the launcher boundary regression and early
rejection fix; they do not establish general cold-start failure. Long-prefix
rejection and short fresh CLI behavior have been checked independently.

Apple Terminal remains a vendor-artifact gate. Use a legitimately acquired,
unmodified Terminal.app and record its source macOS release, app version,
executable hash and architecture before testing shell interaction, resize,
multiple windows, UI/UX, GPU and stability. No Terminal GUI fixture exists in the
pinned sources; a generic terminal is not a substitute compatibility result.

Provenance: vendored libc PTY and xnu termios/ioctl interfaces/source (rung 2),
original fixture and source-built API observations (rung 4). No Apple binary
inspection or stock-application download.
