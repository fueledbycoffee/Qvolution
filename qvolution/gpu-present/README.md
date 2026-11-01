GPU presentation lane tools

These are the source-controlled copies of the helpers in
`Qvolution-work/parallel/gpu-present`. They reuse that lane's component clones,
Stage41c Ninja graph and the existing lane stage overlay; paths are intentionally
host-specific. No network or container access is needed to build the runtime.

Build the committed pin (one build slot, Ninja uses -j6). The stage phase reuses
the existing lane stage and preserves replaced inodes under out/stage-backups;
it never creates a new full stage copy:

```sh
cd /home/sean/Documents/Codex/Projects/Qvolution-work/parallel/gpu-present
../../dashboard/qv-slot build sh -c 'python3 build.py build && python3 build.py stage && python3 build.py fixtures'
```

Run the container/GPU gates on a host that supports them, with no outer slot:

```sh
/home/sean/Documents/Codex/Projects/Qvolution-work/parallel/gpu-present/run-gates.py
```

The runner checks source/stage/fixture receipts, snapshots the six host folders,
uses fresh initialized prefixes, shuts down only its exact prefix, preserves
failed attempts and never deletes artifacts. Correctness runs take guest slots;
only short measurement jobs take perf slots after guest initialization. A timeout
uses exact-prefix shutdown, never a signal or `timeout` command. Measurement
receipts include host load averages after slot acquisition.

Gates: all 71 TextEdit fixtures and theme P4/P5 on shm/EGL-verify; real framebuffer
checks and a negative control; native theme and damage backing captures compared
byte-for-byte with shm; layer move/fade/scroll draw counts, 40000-point documents,
host contents, draw-rectangle queries, default clips, flipped/unflipped gradient/text
captures compared with shm, and zero-stencil depth16 FBO mask pixels;
1000-frame FD/RSS soak; full-window, small-damage and layer timing samples with
verification disabled. Every fixture failure, missing required result, stale
pixel, incomplete soak or host-folder change makes the runner fail.

Native runner classification check: `python3 verify-runner.py`.
Hosted-image upload check (no GPU, ASan/UBSan):

```sh
cd /home/sean/Documents/Codex/Projects/Qvolution-work
ASAN_OPTIONS=detect_leaks=0 dashboard/qv-slot build python3 \
  parallel/gpu-present/cocotron/tests/quartzcore-texture-upload.py \
  --output-dir parallel/gpu-present/r6/native-final
```

The damage fixture keeps local menus with `DARLING_NO_GLOBAL_MENU=1` before
AppKit starts. It asserts requested content, menu frame and decorated backing
sizes so an asynchronous desktop menu handoff cannot silently change the shm/EGL
comparison geometry. The comparison still requires identical capture filenames
and every backing byte. The layer fixture also replaces app-owned RGBA contents
after the first upload and checks the resulting framebuffer colour.

Framework/fixture execution requires Darling and a GPU; compilation alone is not
a passing runtime gate. No result is fabricated when those facilities are absent.
