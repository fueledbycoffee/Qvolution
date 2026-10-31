# Qvolution: 20 Mac application targets

Source TextEdit has partial, user-confirmed workflow evidence; the other 19 application targets are not yet tested in Qvolution. No target has passed all UI/UX, GPU and stability gates. This is the work order, not a promise of compatibility. Catalog versions are discovery metadata, not downloaded or tested artifacts. Pin the actual artifact hash, version, architecture and runtime manifest on each run. Discord is counted once. Dia means The Browser Company browser.

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
