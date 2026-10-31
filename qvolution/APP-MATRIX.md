# Qvolution: 20 Mac application targets

Source TextEdit has partial, user-confirmed workflow evidence. Official KeePassXC, VLC, Transmission, Firefox and VS Code bundles now have bounded launch records; their UI and real workflows remain unverified. No target has passed all UI/UX, GPU and stability gates. This is the work order, not a promise of compatibility. Catalog metadata and tested artifact versions are recorded separately. Pin the actual artifact hash, architecture and runtime on each run. Discord is counted once. Dia means The Browser Company browser.

Current tested inputs: KeePassXC 2.8.0-1 Intel, VLC 3.0.24 Intel64 and Transmission 4.1.4 universal (x86_64 execution target); publisher SHA256 checks passed and executable hashes were unchanged after the recorded runs. Stage6 KeePassXC reached native initialization but stopped at the screen color-space API. Stage6 VLC progressed beyond its earlier PDF cross-reference exception, but still has unverified rendering/playback and audio/PDF/slider diagnostics. Transmission has stage4 UI-initialization evidence and a layout diagnostic, without a stage5/6 retake. Timeouts establish neither a hang nor a working app. TextEdit remains a source-fixture result, not an Apple vendor-bundle pass.
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


## Completion criteria

Every row must pass artifact/architecture, its full workflow, polished UI/UX, verified GPU behavior and stability qualification from [the delivery roadmap](ROADMAP.md). Launch-only and source-fixture results are recorded but do not qualify a vendor application. GPU presentation, composition and rendering are recorded separately so a CPU path cannot silently count as hardware acceleration. Work on shared UI, menu, PTY, GPU and browser/media foundations proceeds in parallel.

Stage7 retake: KeePassXC clears `NSScreen.colorSpace` and stops at missing `NSColorSpace.ICCProfileData` (exit134). It remains blocked during initialization; no UI/workflow promotion.

Stage9 adds publisher-verified Firefox159.0 universal and VSCode1.144.0 Intel inputs. Their earlier import failures clear; Firefox now needs `ASWebAuthenticationSessionErrorDomain`, VSCode needs `LSSetDefaultHandlerForURLScheme`, and KeePassXC needs `CGDisplayIsOnline`. All remain unqualified; no new app pass is recorded.
