# Hex Fiend

Status: **not tested**. Added to the 24-app target list on 2026-10-31.

Source: https://hexfiend.com/

## Acceptance workflow

1. Open a self-authored binary fixture, find a known byte sequence, edit bytes and undo.
2. Save the edited fixture, quit, reopen and compare exact bytes.
3. Open a self-authored large file and scroll through it.

## Desktop and qualification checks

Verify Orbit menus and fallback, keyboard focus, clipboard where applicable, resize and fractional scaling, complete controls and coherent light/dark appearance. Record actual GPU device/backend/output and distinguish CPU rasterization from presentation and rendering. Apply the repeat-launch, workflow and soak gates in [the roadmap](../../ROADMAP.md).

## Evidence

Create a record with `python3 qvolution/qv.py new-run hex-fiend`. Pin the official bundle, hash, architecture and runtime. Use disposable self-authored fixtures. No launch, workflow, UI, GPU or stability result is claimed.

Select and record the exact macOS artifact and architecture at acquisition; no artifact has been acquired or tested.
