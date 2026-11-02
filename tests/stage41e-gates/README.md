# Stage41e gate rerun

Run `bash tests/stage41e-gates/rerun.sh` from the lane clone, or the lane's
`rerun.sh` link. The wrapper requires the lane's existing cp -al overlay and
reviewed input lock, acquires one guest slot and runs 12 Foundation cases,
the 20-second Chess loader gate and Terminal90 last. It creates short fresh prefixes,
initializes before copying, clears inherited DYLD variables, isolates home
folders and checks the host-folder guard. A failed shutdown stops the sequence.

`--dry-run-test-only` is off by default. It executes all preflight checks, input
hash checks and case construction, then stops immediately before the first
container boot. It does not create or signal any prefix. It does not count as
a guest pass. The exact dry-run log and machine-readable preflight are in the
lane's evidence directory.

Native regressions: `cd tests/stage41e-gates && python3 -m unittest -v test_contracts.py`.
Temporary authored fixtures are retained under `/tmp/stage41e-gates-*`.

## Findings

The unchanged strong31 fixture opens
`/Applications/r2-foundation-stub/foundation-strong31-io`. The aggregate harness
only made `/Applications/Stage41e`; the historical prefix is missing the former
parent. The native regression reproduces ENOENT before creating that directory.
No fixture or runtime API needs changing, and home isolation touches only Users.

The five round3/locale fixtures end in `name: passed`, not `PASS`. Require each
exact completion line and exit status. Strong31 now also requires its final
completion, rather than any intermediate PASS. Stub-forwarding's empty root
incorrectly compared the canonical guest path against the staged host mapping
reported by our loader. Supply the staged payload root; retain the fixture's
strict realpath image check and all selector checks unchanged.

Terminal's first false shell probe is allowed by the original gate. The saved
false/true/true probes do not prove a later shell failure. The empty assertion
and absence of observation fields identify the later `ps` deadline branch's
failed shutdown assertion. That gate never finished 90 seconds. Our rerun saves
the partial observations, named checks, each helper shutdown exit, our launcher's
shutdown diagnostics, prefix-scoped process state and library names. It discards
app stdout and retains only Wayland protocol tokens from stderr. Server evidence
contains category counts and fixed numeric process/thread lifecycle records only.

All 11 queued Chess fixes have identical cherry-picks in Stage41e. The final KVO
production files and NSDate source are unchanged from the queued fixes; installed
Foundation, CoreGraphics, GameKit and GeoKit match their Stage41e build products.
The previous Chess handover explicitly leaves a later crash unresolved. Current
metadata says exit 139 after about 12 seconds, with no recorded dyld error. It
does not identify the crash location or establish that it is the same crash.
Raw Chess/Terminal streams remain unread. No runtime fix is inferred from this.

## Remaining verification

The sandbox has no container access, visible host container PIDs or coredumpctl.
The old Terminal runner discarded the shutdown reason and recorded no info-level
server events. The process keeping that prefix alive and the current Chess crash
cause remain unverified. On a capable host, inspect the new named failure and
runtime-only evidence; do not read app streams or infer an API fix from liveness.

No compiled source or installed runtime changes are made by this lane. No new
slice-link claim is needed: the runtime hashes, including the existing kernel,
dyld and framework payloads, must remain exactly those of Stage41e. Never install
into the hardlinked overlay without first replacing affected hardlinks atomically.

The runner prints the milestone command it would add; it never executes it. The
orchestrator may record that milestone only after inspecting the actual guest
result and using that result's evidence path.
