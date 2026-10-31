Qvolution Orbit 1.1.3 offline runtime patch
=========================================

This bundle contains ONLY the five runtime files from the reviewed Orbit
companion patch, a version-locked Python installer, tests and GPL provenance.
It does not install a new extension, change extension settings, stop a daemon,
restart GNOME or require network access. Real top-bar acceptance is pending a
matched Qvolution runtime and an activated patched Orbit session.

The installer accepts exactly these paths:

  extension root: extension.js (modified), windowData.js (added)
  daemon root: globalmenu/menu_model/menu_model.py (modified),
               globalmenu/utils/service.py (modified),
               globalmenu/utils/wayland_menu.py (added)

The extension metadata must identify global-menu@unmade.space version 1.1.3.
Before applying, ALL three existing files must have the exact Orbit 1.1.3
baseline SHA256 hashes compiled into install.py, and the two added paths must
be absent. Unknown contents, symlinks and unrecognized backup state are refused.
Payload hashes are also compiled into the helper. An installed copy of some
other Orbit version or host-local changes must be reviewed separately; this
helper does not overwrite them or fetch another version.

Review-only command (writes nothing):

  python3 install.py apply --dry-run \
    --extension-root /home/sean/.local/share/gnome-shell/extensions/global-menu@unmade.space \
    --daemon-root /home/sean/.local/share/orbit-global-menu \
    --work-root /home/sean/Documents/Codex/Projects/Qvolution-work

The coordinator may run the SAME command without --dry-run after review.
Original bytes, modes and a transaction manifest are retained under
Qvolution-work/backups/orbit/<root-and-payload-identity>/manifest.json. Files
are replaced atomically one at a time, not as one atomic five-file transaction.
The manifest is saved before target writes; an interrupted apply can be rerun
to resume only known baseline/patched states. An already completed apply is
verified and reported as already applied. No backups are deleted.

To preview rollback, replace "apply" by "rollback" and retain --dry-run.
Remove --dry-run to restore. Rollback refuses content or permission changes
since installation and validates original-backup hashes before touching files.
It restores the three original files and removes ONLY this bundle's two added
files. An interrupted rollback can be rerun; finish it before applying again.
Repeated completed rollback verifies the baseline and makes no target changes.
A local edit or damaged backup requires manual review; it is never silently
replaced. Per-file writes are rechecked during both operations.

Tests (only temporary directories are mutated):

  ORBIT_BASELINE_REPO=/path/to/offline/orbit-checkout \
    python3 test_install.py

The offline checkout must contain aa5a9934edf5ee88c651d76a80ecbf936b9500a0.
Tests read its source objects to construct exact temporary baseline fixtures.
The installer itself does not require git or the source checkout. No tests
write to the installed extension/daemon, settings or session state.

Upstream: https://github.com/Unmade760/orbit-global-menu
Version: 1.1.3, source aa5a9934edf5ee88c651d76a80ecbf936b9500a0
Reviewed patch: bec5f1bdd3aa46c836ceeabb8189ea0796741121
Orbit runtime code remains GPL-3.0; full LICENSE.txt is included. See
PROVENANCE.json for the five original/patched file hashes and source paths.
The Qvolution deployment helper and tests are provided under that same GPL-3.0
license. No app binaries, credentials or document contents are included.
