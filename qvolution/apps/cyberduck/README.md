# Cyberduck

Status: **not tested**. Wave 2; priority 10.

Source: https://cyberduck.io/

Use local test credentials, not personal servers.

## Acceptance workflow

1. Connect to a local test SFTP server.
2. Upload and download a fixture.
3. Compare hashes and save a test bookmark.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run cyberduck`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
