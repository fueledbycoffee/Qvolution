# Controlled presentation scene

Use the installed GNOME Shell headless backend to run original framework fixtures away from unrelated desktop windows. This does not install another compositor, change desktop preferences or provide input/screenshot automation. It creates a new session bus, private XDG configuration/data/cache/state, memory GSettings and one virtual monitor. Native services may start on that private bus. Paths and raw logs remain local.

The launcher defaults to a plan. Select an explicit NEW absolute run directory below the private work directory; its parent must already exist. To start:

```sh
python3 qvolution/testing/controlled-compositor.py --run-dir "$QVOLUTION_PRIVATE_RUN" --launch
```

Keep the tracked process running while tests execute. Read only this run's client-env.json. For the original native GTK control, merge those values into its environment and run `gjs -m qvolution/testing/native-frame-control.js`. It draws original CPU/cairo content and never proves GPU acceleration. With WAYLAND_DEBUG=client, independently inspect its matching frame/done and presentation-feedback events.

For a guest fixture, merge the same private environment AFTER runner defaults, use an absolute WAYLAND_DISPLAY made from its XDG_RUNTIME_DIR and recorded socket name, select DARLING_APPKIT_BACKEND=wayland and a dedicated short DPREFIX. Do not use any runner that overwrites this selector with the physical desktop socket. Preserve the same environment for exact-prefix darling shutdown. Never silently fall back to another display. Stop the guest prefix first, then Ctrl-C the exact tracked launcher; it signals only its own child process group. Do not search/terminate compositor processes by name.

An original controlled run passed the GTK scheduling control, all shm capture configuration modes and EGL capture. The unchanged EGL positive fixture passed independent full-frame pixels, updates, resize, remap, alpha, paused flushing, caller context and teardown; deliberate1pixel corruption was rejected. Every guest prefix shut down successfully. The compositor selected a real GBM renderer and the client reported a hardware OpenGL renderer; this establishes an isolated GPU-presentation test, not GPU glyph/path rasterization or any vendor-app qualification.

On the physical session the same fixtures stopped receiving a second frame callback; explicit dispatch and different submission methods did not resolve that observation. The isolated result establishes environment dependence, not the physical cause or verified visible output. Actual GNOME/Orbit/UI acceptance, physical mixed-scale transitions, popup/child integration, performance and stability remain separate gates.

Sources: installed GNOME50.1/mutter50.1 command-line support and primary implementation/documentation; official wl_surface.frame semantics permit callback throttling for invisible/obscured surfaces. The mode and GTK stimuli are original test code. No Apple or vendor binary internals are inspected.
