# Discord

Status: **blocked during loading; latest bounded retake stage 17**. Wave 4; priority 20.

Source: https://discord.com/download

Use the unmodified official Mac client. Passing login is not working Discord; capture APIs are currently incomplete.

## Recorded official input

Discord 0.0.420 is the official stable Mac distribution, preserved as an unthinned x86_64+arm64 universal bundle with x86_64 execution. The source is the [official stable API](https://discord.com/api/download/stable?platform=osx), resolving to the [versioned official CDN archive](https://stable.dl2.discordapp.net/apps/osx/0.0.420/Discord.dmg).

The recorded checks are official HTTPS origin/CDN and CDN MD5 transport match, plus local SHA256 pinning. Publisher SHA256, signature and notarization verification were not performed. Local archive SHA256: `f1f809f79873a375ebaff8dd09858bf1cd9ac91167853a928f747b9f891c8dc8`. The initial stage 13 run left the main executable and all 300 extracted regular-file/symlink entries unchanged against the acquisition manifest; this is not an independent per-member DMG decoder comparison.

Stage 13 stopped at the NSUserActivity class import expected through CoreServices, despite the existing Foundation implementation. Stage 14 clears that re-export gate, then stops at `UNNotificationAttachmentOptionsTypeHintKey` with exit 139; main executable unchanged and exact-prefix shutdown 0. Stage 17 clears the type-hint data import, then stops at `NSScrubberImageItemView` with exit 139; main executable unchanged and exact-prefix shutdown 0. No login/account action, visible UI, message, voice, camera, screen share, GPU or stability qualification is established. Current runtime hashes and historical gates are in [apps.json](../../apps.json). The original stage 13 raw record was overwritten by the stage 14 retake; its retained coordinator observation omits unknown launcher/checkout metadata rather than inferring it from the newer run. Registry API probes and loader progress do not establish working Discord or actual URL delivery.

## Acceptance workflow

1. Render login and authenticate interactively.
2. Navigate servers and exchange an authorized test message.
3. Test voice, then camera, then screen share as separate gates.

## GNOME checks

- Focus, activation, multiple windows and desktop identity.
- Menu actions, disabled/check state, keyboard shortcuts and panel restart fallback.
- Clipboard both directions; resize; 100%, 125%, 150% and 200% scale.
- Clean exit and successful repeat launch in the same dedicated prefix.

## Evidence

Create a run record with `python3 qvolution/qv.py new-run discord`. Fill in actual version, architecture, SHA256, runtime manifest, commands and observed results. Keep authentication secrets and private chat content out of shared logs.
