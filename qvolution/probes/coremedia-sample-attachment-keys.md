# CoreMedia sample attachment data key

Firefox's recorded loader gate requires the public
kCMSampleAttachmentKey_IsDependedOnByOthers data symbol. Export it as a real
immutable CFString key alongside the existing DisplayImmediately implementation,
and declare it in the public CMSampleBuffer header. The already-implemented
NotSync key also gains its missing public declaration; its storage is unchanged.
No other declared-but-missing sample keys were found, so no speculative export
batch or decoder/class implementation is introduced.

Apple's public documentation specifies a CFString dictionary key whose associated
value is CFBoolean. Absence means dependency knowledge is unavailable; explicit
false permits treating the frame as droppable. This is not an instruction to
insert false by default. The constant itself does not compute dependencies or
create/parse/decode samples.

GUESS: the project uses the bare literal `IsDependedOnByOthers`, following its
existing `NotSync` and `DisplayImmediately` spellings. The public documentation
and bounded primary API-client search did not establish native constant bytes.
Chromium's public VideoToolbox client uses the symbol as a dictionary key, but
not a literal replacement for it. The fixture's exact bare-string check verifies
this explicit project policy, not Mac measurement or serialized native-media
interoperability. No Apple SDK/binary or vendor resource was inspected.

The original .c fixture dynamically opens the actual framework. Existing
DisplayImmediately and NotSync CFString data are positive controls before the
missing-symbol check, so it compiles on the parent. Candidate checks real CFString
type/nonempty/distinct keys and a generic original CFDictionary roundtrip with
separate absent/false/true states and a retained dictionary snapshot. This is
ordinary metadata storage only. No CMSampleBuffer/codec function is called;
existing unsupported sample-buffer/media paths are not made functional by this
constant. A data-contract pass is not a Firefox launch, playback, GPU or app pass.

Copied real production-object recipe and original fixture compile/link. The
unchanged parent failed the new data-symbol check after both existing positive
controls: exit1 in1.612seconds, exact-prefix shutdown0. Candidate contract and
unchanged official Firefox retake remain coordinator staging gates. Actual sample attachment propagation, native key-value fidelity
and media decoding remain unqualified.

Sources: repository CMSampleBuffer/NotSync neighbors (rung2), public Apple API
(rung3), and original public-boundary fixture (rung4 after execution):
https://developer.apple.com/documentation/coremedia/kcmsampleattachmentkey_isdependedonbyothers
Public client (symbol use only, not literal evidence):
https://chromium.googlesource.com/chromium/src/+/HEAD/media/gpu/mac/vt_video_encode_accelerator_mac.mm
