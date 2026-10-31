# Blender

Status: **not tested**. Added to the 24-app target list on 2026-10-31.

Source: https://www.blender.org/

## Acceptance workflow

1. Create a small scene, navigate the viewport and manipulate an object.
2. Save, quit, reopen and verify the scene.
3. Render the scene and separately verify the actual GPU device used.

## Desktop and qualification checks

Verify Orbit menus and fallback, keyboard focus, clipboard where applicable, resize and fractional scaling, complete controls and coherent light/dark appearance. Record actual GPU device/backend/output and distinguish CPU rasterization from presentation and rendering. Apply the repeat-launch, workflow and soak gates in [the roadmap](../../ROADMAP.md).

## Evidence

Create a record with `python3 qvolution/qv.py new-run blender`. Pin the official bundle, hash, architecture and runtime. Use disposable self-authored fixtures. No launch, workflow, UI, GPU or stability result is claimed.

Catalog version is a verified 4.5 LTS release with an Intel macOS download, not a claim of the latest Blender release. Select and record the exact artifact and architecture at acquisition. A completed render alone does not establish GPU qualification; record actual device evidence separately.
