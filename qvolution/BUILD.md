# Build and test Qvolution

The initial source baseline is VibeDarling commit `1972ae4bc79b96f3dbb952d3bc658beb96752103`. Preserve the exact submodule commits; do not replace them all with branch tips to make a build succeed.

## Ubuntu 26.04 dependencies

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
cmake -S . -B ../Qvolution-work/build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr/local -DENABLE_TESTS=ON
cmake --build ../Qvolution-work/build --parallel 8
ninja -C ../Qvolution-work/build -n
```

A successful dry run must report no remaining work. A clean compiler exit alone is insufficient if the graph regenerated. Do not run two heavy builds in this workspace at once.

## Stage without installing system-wide

After committing intended source changes and recording a source manifest:

```sh
DESTDIR="$(realpath ../Qvolution-work)/stage" cmake --install ../Qvolution-work/build
DPREFIX="$(realpath ../Qvolution-work)/prefixes/cli-smoke" DARLING_INSTALL_PREFIX="$(realpath ../Qvolution-work)/stage/usr/local"   ../Qvolution-work/build/src/startup/darling shell /bin/echo Qvolution
```

Run `python3 qvolution/cli-smoke.py` after staging to check three nonroot boot/shutdown cycles in the dedicated `cli-smoke` prefix. It records stdout, errors and cleanup results in the sibling workspace.

Use the build tree's non-setuid launcher. Stop that exact prefix with the same environment and `darling shutdown`. Do not use process-name kill loops or treat a timeout as clean shutdown. Check the source manifest, repeated startup and shutdown before GUI testing.

## Application results

Use `python3 qvolution/qv.py new-run APP_ID` for an evidence template. Acquisition/install/load/launch/workflow/GNOME/stability are independent gates. Store exact bundle hashes and architecture. App-specific reports start under `qvolution/apps/`. A source-built fixture and an unmodified vendor artifact are different results. Never publish app binaries or credentials with the source repository.

The existing sandbox entry points do not establish Mac-equivalent confinement. Track browser sandbox behavior explicitly and use disposable test data until it is understood. Authentication and message/call actions remain interactive and require a chosen test destination.
