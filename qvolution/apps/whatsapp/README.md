# WhatsApp

Status: **not tested**. Wave 4; priority 18.

Source: https://www.whatsapp.com/download/

Requires user interaction and an explicitly chosen test contact for message/call tests.

## Acceptance workflow

1. Render the account-linking flow.
2. Link interactively and exchange an authorized test message.
3. Test a fixture attachment, notifications and separately a call.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run whatsapp`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
