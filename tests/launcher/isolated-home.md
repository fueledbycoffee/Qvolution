# Prefix home isolation integration

The darlingserver pin introduces `DARLING_ISOLATED_HOME=1`; the launcher's
root-mode environment allowlist forwards it. Only exact `1` changes setup.
The server creates real home directories and refuses symlink/file conflicts.
Use a fresh prefix and stop the server before changing the option. Normal
user launches remain unchanged when the variable is absent.

Catalyst corefoundation dadcb5c / superproject 8ea6e06d5f implements the separate
`DARLING_APP_CONTAINER=1` option. It selects a per-bundle CF home, whereas this
option protects all prefix home-folder writes, including guest shell commands.
It does not restrict deliberate access through `/Volumes/SystemRoot`.

`dashboard-isolated-home.patch` records the out-of-repository fleet integration.
Apply from Qvolution-work with `patch -p1 --forward < <patch-path>` only if it is
not already present. Dashboard guest slots default to isolation; explicit `0`
preserves normal behavior for controls. Keep qv-host-guard snapshot/verify for
all runs, including failed commands and shutdown. Older runtime builds ignore
the new environment variable; rebuild the launcher and darlingserver before
relying on it. Native server tests are `python3 tests/isolated-home.py` in the
pinned component checkout. Actual container validation remains a host gate.
