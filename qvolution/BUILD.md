# Build and test Qvolution

The initial source baseline is VibeDarling commit `1972ae4bc79b96f3dbb952d3bc658beb96752103`. Preserve the exact submodule commits; do not replace them all with branch tips to make a build succeed.

## Ubuntu 26.04 dependencies

A fresh checkout needs two remotes. Most upstream submodule URLs are relative
siblings, so keep `origin` on VibeDarling and use `qvolution` for this project.
The Qvolution component fork URLs are explicit and stay pinned by the gitlinks.

```sh
git clone --branch qvolution/parallel-desktop https://github.com/fueledbycoffee/Qvolution.git
cd Qvolution
git remote rename origin qvolution
git remote add origin https://github.com/VibeDarling/darling.git
```

The cloned branch keeps tracking the renamed `qvolution` remote. Fetch/pull this
project from `qvolution`; the upstream `origin` supplies relative dependency URL
resolution. Do not initialize these relative submodules against a personal fork
that does not contain their sibling repositories.

Review and run `bash qvolution/bootstrap-ubuntu.sh`. It uses Ubuntu packages and sudo, and lets apt ask for confirmation. Then:

```sh
git submodule update --init --recursive --jobs 4
git -C src/external/swift lfs pull
python3 qvolution/qv.py doctor
python3 qvolution/qv.py host-tests
```

## Configure and build

Run from the repository root. The sibling workspace keeps build artifacts and prefixes outside the source tree:

```sh
cmake -S . -B ../Qvolution-work/build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_TESTS=ON -DQVOLUTION_BUILD_TEXTEDIT=ON
cmake --build ../Qvolution-work/build --parallel 8
ninja -C ../Qvolution-work/build -n
```

A successful dry run must report no remaining work. A clean compiler exit alone is insufficient if the graph regenerated. Do not run two heavy builds in this workspace at once.

## Stage without installing system-wide

After committing intended source changes and recording a source manifest:

```sh
DESTDIR="$(realpath ../Qvolution-work)/stage" cmake --install ../Qvolution-work/build
DPREFIX="$(realpath ../Qvolution-work)/prefixes/cli-smoke" DARLING_INSTALL_PREFIX="$(realpath ../Qvolution-work)/stage/usr/local" DARLING_NONROOT=1 ../Qvolution-work/build/src/startup/darling shell /bin/echo Qvolution
```

Run `python3 qvolution/cli-smoke.py` after staging to check three nonroot boot/shutdown cycles in the dedicated `cli-smoke` prefix. It records stdout, errors and cleanup results in the sibling workspace.

Use the build tree's non-setuid launcher. Stop that exact prefix with the same environment and `darling shutdown`. Do not use process-name kill loops or treat a timeout as clean shutdown. Check the source manifest, repeated startup and shutdown before GUI testing.

## Application results

Use `python3 qvolution/qv.py new-run APP_ID` for an evidence template. Acquisition/install/load/launch/workflow/GNOME/stability are independent gates. Store exact bundle hashes and architecture. App-specific reports start under `qvolution/apps/`. A source-built fixture and an unmodified vendor artifact are different results. Never publish app binaries or credentials with the source repository.

The existing sandbox entry points do not establish Mac-equivalent confinement. Track browser sandbox behavior explicitly and use disposable test data until it is understood. Authentication and message/call actions remain interactive and require a chosen test destination.

## Source TextEdit fixture

`QVOLUTION_BUILD_TEXTEDIT` defaults OFF and requires AppKit/the gui component when enabled. The target builds the pinned open-source TextEdit submodule, not a current Apple-distributed binary. Its staged executable is `/Applications/TextEdit.app/Contents/MacOS/TextEdit`. Keep fixture results separate from vendor-bundle compatibility.


Normal x86_64 Objective-C startup leaves `DARLING_ENABLE_LEGACY_X86_SWIFT_FOUNDATION_OVERLAY` OFF (the default). The bundled x86 Swift 5.2.2 overlay cannot be eagerly re-exported against the 6.3.3 runtime. This does not implement Swift ABI compatibility; apps that explicitly need that overlay remain unsupported. The arm64 overlay path is unchanged and was not runtime-tested here.

After staging, launch the source fixture from your desktop session without any `DYLD_*` overrides:

```sh
DPREFIX="$(realpath ../Qvolution-work)/prefixes/textedit-normal" DARLING_INSTALL_PREFIX="$(realpath ../Qvolution-work)/stage/usr/local" DARLING_NONROOT=1 DARLING_APPKIT_BACKEND=wayland ../Qvolution-work/build/src/startup/darling shell /Applications/TextEdit.app/Contents/MacOS/TextEdit
```

The focused normal-startup target is `foundation_objc_startup`, under `build/src/external/foundation/test/objc-startup/`. Run it inside the guest via `/Volumes/SystemRoot` followed by its absolute host path, with all `DYLD_*` overrides removed. It rejects diagnostic overrides and checks normal Foundation initialization and a Unicode JSON round trip. `qvolution_titlebar_policy 0`, `qvolution_presentation_atomicity`, `qvolution_save_browser_snapshot FIXTURE_DIRECTORY` and `qvolution_save_browser_expansion` have also passed on Wayland without diagnostic overrides; their component test notes describe fixtures and cleanup. These are framework and source-fixture results, not a Swift-app compatibility claim.
