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

## Historical fork checkpoint (through Stage 12)

Retained from `65b294ed1` for provenance. These statements describe the earlier checkpoint; the local Stage40 records and later stage-labelled evidence above and in apps.json take precedence. They do not establish new retakes or regress later results.

### Historical 20-target work order

Source TextEdit has partial, user-confirmed workflow evidence. Seven official bundles—KeePassXC, VLC, Transmission, Firefox, VS Code, CotEditor and Keka—have bounded launch records; their UI and real workflows remain unverified. No target has passed all UI/UX, GPU and stability gates. This is the work order, not a promise of compatibility. Catalog metadata and tested artifact versions are recorded separately. Pin the actual artifact hash, architecture and runtime on each run. Discord is counted once. Dia means The Browser Company browser.

Evidence checkpoint: **through stage 12**. The latest retained launch stage varies by app; stages below are not inferred retakes. All seven official archives have publisher SHA256 verification and unchanged executable hashes after their recorded runs. Universal bundles remain unthinned, targeting x86_64. No vendor UI/workflow, GPU or stability pass is established. TextEdit remains a partial source fixture.

| Official input | Latest retained launch | Current recorded gate |
| --- | --- | --- |
| KeePassXC 2.8.0-1 Intel | stage 12 | Earlier placeholder selector cleared; unexplained startup exit 139 without an exception reason. Cause unknown. |
| Firefox 159.0 universal | stage 12 | XPCOM load stops at missing `kCMTimingInfoInvalid`; earlier web-authentication/dependency-key imports cleared. |
| CotEditor 7.1.3 universal | stage 11 | Missing `AppIntents.framework` dependency image, exit 139. |
| Keka 1.6.8 universal | stage 11 | Distributed-notification transport initialization exception reports errno 48, exit 134. |
| Visual Studio Code 1.144.0 Intel | stage 9 | Missing `LSSetDefaultHandlerForURLScheme`, exit 139; no later app retake recorded. |
| VLC 3.0.24 Intel64 | stage 6 | Earlier PDF cross-reference exception cleared; rendering/playback remain unverified with audio/PDF/slider diagnostics and a bounded timeout. |
| Transmission 4.1.4 universal | stage 4 | Interface/layout diagnostics and a bounded timeout; controlled transfer/rendering remain unverified. |

Every recorded exact-prefix shutdown returned 0; that is cleanup evidence, not clean app exit or workflow success. Timeouts establish neither a hang nor a working app. Stage12 own-source semantic-color/opacity and CoreMedia data probes pass independently; they do not qualify KeePassXC or Firefox. Historical stage-labelled records remain in apps.json.

| Order | App | Wave | First useful workflow |
| --- | --- | --- | --- |
| 1 | TextEdit | 1 | Create a Unicode document; Edit, undo, copy and paste |
| 2 | Terminal | 1 | Open a shell and run pwd and printf; Test PTY input, Unicode and resize |
| 3 | Stickies | 1 | Create and edit two notes; Copy and paste between windows |
| 4 | Calculator | 1 | Compute known arithmetic results; Use keyboard input and mode/menu changes |
| 5 | Preview | 1 | Open a local PNG and a multipage PDF; Zoom, navigate and select text |
| 6 | Chess | 2 | Render the board; Play several legal moves |
| 7 | CotEditor | 2 | Open and edit a UTF-8 source file; Find, replace, undo and save |
| 8 | Transmission | 2 | Open a local test torrent; Transfer a self-authored fixture on an isolated peer setup |
| 9 | Keka | 2 | Create an archive of known fixture files; Extract into a fresh directory |
| 10 | Cyberduck | 2 | Connect to a local test SFTP server; Upload and download a fixture |
| 11 | VLC | 2 | Play a local sample with audio; Seek, pause and toggle fullscreen |
| 12 | IINA | 2 | Play a local sample with audio; Seek and render subtitles |
| 13 | KeePassXC | 2 | Create a dummy vault; Add and copy a dummy password |
| 14 | Visual Studio Code | 3 | Open a fixture folder and edit a file; Use search and save/reopen |
| 15 | Obsidian | 3 | Create a disposable local vault; Edit and link two notes |
| 16 | Firefox | 3 | Open a local HTML fixture and an HTTPS page; Type, scroll and open a second tab |
| 17 | Spotify | 4 | Render the login flow; Authenticate interactively when ready |
| 18 | WhatsApp | 4 | Render the account-linking flow; Link interactively and exchange an authorized test message |
| 19 | Dia | 4 | Launch on a matching ARM64 runtime/host; Open tabs and render local/HTTPS pages |
| 20 | Discord | 4 | Render login and authenticate interactively; Navigate servers and exchange an authorized test message |

Waves: 1 = basic desktop workflows; 2 = framework/media breadth; 3 = multiprocess editors/browsers; 4 = authenticated services and architecture stretch targets. Chess is explicitly a graphics stretch target.

Dia is ARM64-only in the checked catalog; the initial host is x86_64. It needs an ARM64 host/runtime or a separately validated CPU-translation solution. Darling API translation itself does not supply that CPU translation.

See [apps.json](apps.json) for official URLs, catalog metadata, the full workflows and acquisition requirements.


### Historical completion criteria

Every row must pass artifact/architecture, its full workflow, polished UI/UX, verified GPU behavior and stability qualification from [the delivery roadmap](ROADMAP.md). Launch-only and source-fixture results are recorded but do not qualify a vendor application. GPU presentation, composition and rendering are recorded separately so a CPU path cannot silently count as hardware acceleration. Work on shared UI, menu, PTY, GPU and browser/media foundations proceeds in parallel.

Historical stage 7 retake: KeePassXC cleared `NSScreen.colorSpace` and stopped at missing `NSColorSpace.ICCProfileData` (exit134). It remains blocked during initialization; no UI/workflow promotion.

Historical stage 9 added publisher-verified Firefox159.0 universal and VSCode1.144.0 Intel inputs. Their earlier import failures cleared; Firefox then needed `ASWebAuthenticationSessionErrorDomain`, VSCode needed `LSSetDefaultHandlerForURLScheme`, and KeePassXC needed `CGDisplayIsOnline`. All remain unqualified; no new app pass is recorded.
