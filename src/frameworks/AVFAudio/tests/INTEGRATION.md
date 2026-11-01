# Combined media ownership

This lane's final commits replace the overlapping Firefox speech commit
`18950b1076efbd8a4e8ea316448b1eb30802755c` and Electron CoreVideo commit
`9471ef2a9a321ea2969571cc361edee52e8d99c4`. Do not cherry-pick those two original
commits after this series. Their unique API is incorporated here.

AVFAudio keeps one implementation of each speech class, in AVSpeech.m.
Both source lists must not be combined. Utterance text/copying, voice, rate,
pitch, volume and delays are shared API; defaults are 0.5, 1, 1, zero, zero.
The numeric defaults remain GUESS. Media's allocated unsupported synthesizer
with cancellation delegate callback is selected over Firefox's nil constructor;
speaking/paused and control results stay false. Media also retains rate constants,
delegate declarations and routing arbitration. No Firefox-only selector is lost.

CoreVideo keeps Media's single pool flush, three constants and clean rectangle
implementation, plus Electron's unique attachment-presence and compression APIs.
Malformed clean-aperture dictionaries, including missing either dimension, yield
the full encoded rectangle; do not combine Electron's encoded-dimension defaults
with this policy. Both lanes' valid dimensions/offsets, invalid metadata and null
tests are retained, with explicit missing-dimension regressions.

The overlapping CM, VT, playback-date and JPEG commits named in REVIEW-codex.md
also add definitions already present in this series; integrate each definition
once. This is a source union and an integration replacement plan, not evidence of
a performed merge into the orchestrator's checkout or a passed guest gate.
