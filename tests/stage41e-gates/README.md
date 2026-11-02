# Stage41e gate rerun

Round 3 uses `rerun-r3.sh`: twelve Foundation gates, Chess20 and two Terminal90
cases. The Terminal launchers differ only by reversing d0872f92427's stdio change;
both retain the verified shutdown fix. Init, app and ps launches use the selected
variant, and cleanup always uses the detached launcher. The inherited-stdio case
can fail the unchanged pipe-EOF gate; that is evidence, not a relaxed timeout.
Run the same wrapper with `--dry-run-test-only` to validate everything before boot.

The three changed framework payloads replace only this lane's files, preserving
the original hardlinks as evidence. Their full universal links include i386 and
x86_64; no libobjc/libsystem/dyld or static variant changes. The patch receipt pins
each replacement and its original backup. The synthetic qvAudit selectors remain
unknown and must throw, while real BOOL state and command getters are also tested.
Installed sources still contained catch-all forwarding; changing that expectation
would conceal the defect. Other framework placeholder APIs are outside this fix.

Run `bash tests/stage41e-gates/rerun.sh` from the lane clone, or the lane's
`rerun.sh` link. Round 2 uses `rerun-r2.sh`. The wrapper requires the lane's existing cp -al overlay and
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
reported by our loader. Supply the staged payload root. The round-2 fixture verifies byte identity of the
mapped file against that exact payload: nonroot prefixes copy files, so realpath
identity is too strict. Changed or missing copies fail; selector checks remain.

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

Round 2 builds only the Linux launcher and our authored stub-forwarding fixture
in the lane's `build-r2`. The stage remains unchanged. `--patch-receipt` pins the
patch commit, source hashes, build recipe, launcher and fixture; `--input-lock`
pins that receipt alongside all baseline inputs. No libobjc, libsystem or dyld
source changes are made: all existing library slices retain their payload hashes.

Round-2 native controls exercise a launcher that exits while a child holds stdout,
detached daemon stdio with debug flags and initially closed descriptors, and a
reparented session leader that holds a PTY and ignores TERM. The launcher now
uses the existing exact socket/uid/executable orphan predicate before the TERM
grace period while the server is live, too. No process-name fallback is used.
The PTY of another prefix survives. The original header fails this regression.

The launcher always redirects daemon stdin/stdout to /dev/null and stderr to the
prefix owner's `private/var/log/launcher-daemons.log`; launchd inherits those
redirected descriptors from darlingserver. This log can contain guest output and
must not be read for Terminal or Chess. `DSERVER_LOG_STDERR=0` no longer prevents
redirection. Both probe and protocol pipes have deadlines and explicit EOF results;
a shutdown helper deadline fails the run without signalling it or booting more.
The real Terminal shutdown cause still requires the host rerun; the native PTY
regression proves a selection bug, not the cause of the saved host failure.

The runner prints the milestone command it would add; it never executes it. The
orchestrator may record that milestone only after inspecting the actual guest
result and using that result's evidence path.
