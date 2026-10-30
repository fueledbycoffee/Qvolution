# Qvolution

Experimental macOS application compatibility for Linux and GNOME, based on [VibeDarling](https://github.com/VibeDarling/darling) and [Darling](https://github.com/darlinghq/darling).

The first objective is a reliable TextEdit workflow with native GNOME integration. Terminal follows. Mac Electron compatibility is a separate gate before Discord; Dia needs an ARM64 execution path. The source-built TextEdit fixture has user-confirmed ASCII save/reopen and menu/scroll flicker corrections, and now opens normally without diagnostic loader flags. Unicode editing, undo, clipboard, global menus and extended stability remain separate checks; vendor Mac app bundles are not yet validated.

- [20-app matrix](qvolution/APP-MATRIX.md)
- [Work queue](qvolution/ROADMAP.md)
- [Build and run guide](qvolution/BUILD.md)
- [Machine-readable target list](qvolution/apps.json)
- [Original upstream README](https://github.com/VibeDarling/darling/blob/1972ae4bc79b96f3dbb952d3bc658beb96752103/README.md)

## Local commands

```sh
python3 qvolution/qv.py apps
python3 qvolution/qv.py doctor
python3 qvolution/qv.py host-tests
python3 qvolution/qv.py manifest
python3 qvolution/qv.py new-run textedit
```

`origin` stays pointed at VibeDarling so relative submodule URLs resolve correctly. `qvolution` is the push remote for fueledbycoffee/Qvolution; the development branch is `qvolution/main`. Keep every submodule pinned. Build outputs, prefixes and local test evidence live in the sibling `Qvolution-work` directory, outside this repository.

The original GPL license, notices and individual component licenses remain in force. Follow [AGENTS.md](AGENTS.md) for clean-room boundaries. The upstream contribution protocol applies when contributing to VibeDarling; Qvolution local work is tracked in its own queue and does not require posting to upstream.
