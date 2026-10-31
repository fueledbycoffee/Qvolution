# CATapDescription configuration controls

`coreaudio-tap-description.m` is an original typed public client of the genuine
CoreAudio configuration class. It never creates an audio tap/aggregate device,
starts IO, captures audio, requests permission, or changes host audio settings.
Its normal class import must resolve through the framework in an integrated run.

Two modes separate documented model behavior from explicitly chosen defaults:

| Mode | Controls |
| --- | --- |
| `model` | Six process include/exclude, mono/stereo and hardware-stream initializers; mutable process input copy; copied array/string property replacement; UUID/NSNumber ownership; nullable hardware selection; boolean setters/getters; numeric mute roundtrip and documented unmuted default. |
| `project-defaults` | Empty processes/bundleIDs/name, fresh per-instance UUID, nil hardware UID/index, false flags, unmuted. These are conservative project defaults (rung6/GUESS) where Apple does not specify values. |

Require explicit `PASS_CATAPDESCRIPTION mode=...`, fixture exit0, no timeout and
verified exact-prefix shutdown0 in both modes. The fixture uses real typed calls
and rejects compiler selector/pointer errors. It does not use a dynamic success
fallback, substitute class, or protocol stub. No NSCopying/NSCoding conformance is
advertised or tested: the public Objective-C class documentation only establishes
NSObject inheritance. Missing selectors remain missing.

The first source/object was frozen before implementation against Stage31:
compilation passed and normal linkage failed solely on
`_OBJC_CLASS_$_CATapDescription`. That original command contained later inherited
flags that overrode three intended strict diagnostic categories. The supplemental
`before-catap-stage31-strict-v2` attempt puts all four strict error flags last;
compilation passes and normal linkage still fails solely on the missing class.
Its frozen header, fixture source and object hashes are identical to the original.
Preserve both original attempts and runtime metadata when producing after evidence. A source-control
executable may link the unchanged frozen fixture object with the newly compiled
own class object against existing Foundation. Such a result tests the own data
model, and does not establish that the installed CoreAudio exports the class.
After reviewed integration, normally relink that same original object against the
new CoreAudio without including the class object; record new commands, runtime
manifest and binary hash separately, then execute both modes.

The implementation stores configuration through genuine atomic copied properties.
It reuses Foundation arrays/strings/NSUUID/NSNumber and compiler-generated atomic
accessors. Initializer process arrays preserve their order and contents; property
copying is shallow container copying, as declared by the API. Mono/stereo and
include/exclude flags describe the requested configuration; they do not implement
mixing, muting or process capture. A selected hardware stream retains its UID and
NSNumber index and requests that stream's format rather than mono/stereo mixdown.
No tap create/destroy API or fabricated tap ID is introduced by this class.

Primary-source provenance:

- [Apple CATapDescription](https://developer.apple.com/documentation/coreaudio/catapdescription?language=objc) and its individual property/method pages specify the six initializers, exact getters/setters and atomic/copy ownership. In particular `privateTap` uses `isPrivate`/`setPrivate:`, and the numeric `muteBehavior` property uses the getter `isMuted`.
- [Apple CATapMuteBehavior](https://developer.apple.com/documentation/coreaudio/catapmutebehavior?language=objc) specifies the unmuted default. Numeric mute values0/1/2 are inferred from Apple's official sample `Model.swift` `TapMute` and its rawValue conversion to/from `CATapMuteBehavior` in `AudioTap.swift`.
- [Apple's public audio-tap sample](https://developer.apple.com/documentation/coreaudio/capturing-system-audio-with-core-audio-taps) supplies that source example. Its UI configuration defaults are not claimed to be CATapDescription's native defaults.

No class/header was available in this tree before the change. Declarations derive
from those public API documents; no proprietary binary or SDK implementation was
read. The configuration object alone does not qualify the Linux audio-tap,
Pulse/capture-portal or application-audio backend, which remain separate work.
