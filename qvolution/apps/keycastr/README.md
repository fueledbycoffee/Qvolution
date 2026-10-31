# KeyCastr

Status: **not tested**. Added to the 24-app target list on 2026-10-31.

Source: https://github.com/keycastr/keycastr

## Acceptance workflow

1. Enable visualization during an authorized test session and type a known test sequence.
2. Verify the overlay and configured shortcut while changing focus between test windows.
3. Disable visualization, exit cleanly and verify capture stops.

## Desktop and qualification checks

Verify Orbit menus and fallback, keyboard focus, clipboard where applicable, resize and fractional scaling, complete controls and coherent light/dark appearance. Record actual GPU device/backend/output and distinguish CPU rasterization from presentation and rendering. Apply the repeat-launch, workflow and soak gates in [the roadmap](../../ROADMAP.md).

## Evidence

Create a record with `python3 qvolution/qv.py new-run keycastr`. Pin the official bundle, hash, architecture and runtime. Use disposable self-authored fixtures. No launch, workflow, UI, GPU or stability result is claimed.

Limit input observation to the authorized test session; do not capture background personal input. Select and record the exact macOS artifact and architecture at acquisition.
