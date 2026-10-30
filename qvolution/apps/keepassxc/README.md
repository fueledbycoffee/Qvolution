# KeePassXC

Status: **not tested**. Wave 2; priority 13.

Source: https://keepassxc.org/

Use synthetic vault contents only during compatibility testing.

## Acceptance workflow

1. Create a dummy vault.
2. Add and copy a dummy password.
3. Save, lock, reopen and verify the dummy entry.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run keepassxc`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
