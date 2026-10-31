# Qvolution: 24 Mac application targets

The original 20 targets now include Hex Fiend, Skim, KeyCastr and Blender. Every target must complete its recorded workflow with polished macOS-like UI/UX, verified GPU behavior and stability. Catalog versions are acquisition candidates; the actual tested artifact is pinned separately.

Eleven official inputs have bounded launch records through Stage 30. None is fully qualified. Source TextEdit is separate from official vendor TextEdit. Exact-prefix shutdown is cleanup evidence; a timeout or an import correction is not a passing application workflow.

| Order | App | Wave | Latest recorded evidence | First useful workflow |
| --- | --- | --- | --- | --- |
| 1 | TextEdit | 1 | Partial source fixture | Create a Unicode document |
| 2 | Terminal | 1 | Not tried | Open a shell and run pwd and printf |
| 3 | Stickies | 1 | Not tried | Create and edit two notes |
| 4 | Calculator | 1 | Not tried | Compute known arithmetic results |
| 5 | Preview | 1 | Not tried | Open a local PNG and a multipage PDF |
| 6 | Chess | 2 | Not tried | Render the board |
| 7 | CotEditor | 2 | stage11 · blocked loading | Open and edit a UTF-8 source file |
| 8 | Transmission | 2 | stage4 · partial initialization | Open a local test torrent |
| 9 | Keka | 2 | stage22 · partial client render | Create an archive of known fixture files |
| 10 | Cyberduck | 2 | stage30 · blocked initialization | Connect to a local test SFTP server |
| 11 | VLC | 2 | stage6 · partial initialization | Play a local sample with audio |
| 12 | IINA | 2 | stage30 · blocked loading | Play a local sample with audio |
| 13 | KeePassXC | 2 | stage30 · blocked initialization | Create a dummy vault |
| 14 | Visual Studio Code | 3 | stage26 · blocked loading | Open a fixture folder and edit a file |
| 15 | Obsidian | 3 | stage30 · blocked loading | Create a disposable local vault |
| 16 | Firefox | 3 | stage22 · blocked loading | Open a local HTML fixture and an HTTPS page |
| 17 | Spotify | 4 | Not tried | Render the login flow |
| 18 | WhatsApp | 4 | Not tried | Render the account-linking flow |
| 19 | Dia | 4 | Not tried | Launch on a matching ARM64 runtime/host |
| 20 | Discord | 4 | stage26 · blocked loading | Render login and authenticate interactively |
| 21 | Hex Fiend | 2 | Not tried | Open a self-authored binary fixture, find a known byte sequence, edit bytes and undo |
| 22 | Skim | 2 | Not tried | Open a disposable multipage PDF, search known text and zoom |
| 23 | KeyCastr | 3 | Not tried | Enable visualization during an authorized test session and type a known test sequence |
| 24 | Blender | 4 | Not tried | Create a small scene, navigate the viewport and manipulate an object |

## Current startup limits

IINA’s first Stage 30 attempt stops at the missing PIP framework; Obsidian stops at the CoreAudio block-listener import; Cyberduck exits 134 with cause unknown. KeePassXC repeats exit 139 with cause unknown. Discord and VS Code’s latest retained app attempts are Stage 26; their CoreAudio import gate is being addressed independently. No later launch is inferred from framework tests.

Publisher and archive verification differ by artifact. Consult [the authoritative catalog](apps.json) and private artifact manifests: Cyberduck has publisher MD5 verification; Discord has official HTTPS/CDN MD5 and a locally pinned SHA256. A local hash does not establish a publisher signature or notarization.

## Completion criteria

Apply every gate in [the delivery roadmap](ROADMAP.md) to every row: official artifact/architecture, real workflow and persistence, complete UI/UX and Orbit integration, actual GPU output/device evidence, and repeated workflows/soaks. Treat GPU presentation, composition and rendering separately. Histories and stage-labelled observations remain in apps.json and verification records.

Dia requires a verified ARM64 execution path; Darling API compatibility alone does not translate CPU instructions. Blender’s catalog selection is the verified Intel-capable 4.5 LTS line. KeyCastr tests use authorized self-authored input only; Hex Fiend uses self-authored binary fixtures.
