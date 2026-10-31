# Skim

Status: **not tested**. Added to the 24-app target list on 2026-10-31.

Source: https://skim-app.sourceforge.io/

## Acceptance workflow

1. Open a disposable multipage PDF, search known text and zoom.
2. Add annotations and highlights, then save and quit.
3. Reopen and verify annotations, highlights and page content persist.

## Desktop and qualification checks

Verify Orbit menus and fallback, keyboard focus, clipboard where applicable, resize and fractional scaling, complete controls and coherent light/dark appearance. Record actual GPU device/backend/output and distinguish CPU rasterization from presentation and rendering. Apply the repeat-launch, workflow and soak gates in [the roadmap](../../ROADMAP.md).

## Evidence

Create a record with `python3 qvolution/qv.py new-run skim`. Pin the official bundle, hash, architecture and runtime. Use disposable self-authored fixtures. No launch, workflow, UI, GPU or stability result is claimed.

Select and record the exact macOS artifact and architecture at acquisition; use a disposable PDF fixture.
