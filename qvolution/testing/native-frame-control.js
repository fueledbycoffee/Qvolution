// Original GTK scheduling control; CPU/cairo, never hardware qualification.
// Run only against our dedicated headless compositor, never the real desktop.
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Gtk from 'gi://Gtk?version=4.0';

if (GLib.getenv('WAYLAND_DISPLAY') !== 'qvolution-controlled' ||
    !(GLib.getenv('XDG_RUNTIME_DIR') ?? '').startsWith('/tmp/qv-wl-'))
    throw new Error('Dedicated Qvolution compositor environment required');
GLib.setenv('GDK_BACKEND', 'wayland', true);
GLib.setenv('GSK_RENDERER', 'cairo', true);
const app = new Gtk.Application({
    application_id: 'org.qvolution.NativeFrameControl',
    flags: Gio.ApplicationFlags.NON_UNIQUE,
});
let result = 1;
let ticks = 0;
let draws = 0;
let deadline = 0;
app.connect('activate', () => {
    const window = new Gtk.ApplicationWindow({application: app,
        default_width: 96, default_height: 64, decorated: false});
    const area = new Gtk.DrawingArea({content_width: 96, content_height: 64});
    area.set_draw_func((_area, cr, width, height) => {
        ++draws;
        cr.setSourceRGB(ticks % 2, 0, 1 - ticks % 2);
        cr.paint();
        cr.setSourceRGB(0, 1, 0);
        cr.rectangle(0, 0, width / 3, height / 4);
        cr.fill();
    });
    area.add_tick_callback((widget, clock) => {
        ++ticks;
        widget.queue_draw();
        if (ticks === 1 || ticks % 10 === 0)
            print(`NATIVE_CONTROL ticks=${ticks} draws=${draws} mapped=${widget.get_mapped()} frameCounter=${clock.get_frame_counter()}`);
        if (ticks >= 30 && draws >= 20 && widget.get_mapped()) {
            result = 0;
            print('NATIVE_CONTROL completed; require matching Wayland frame/done trace, not a GPU result');
            app.quit();
            return GLib.SOURCE_REMOVE;
        }
        return GLib.SOURCE_CONTINUE;
    });
    window.set_child(area);
    window.present();
    deadline = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 10, () => {
        deadline = 0;
        printerr(`FAIL native frame control timeout ticks=${ticks} draws=${draws}`);
        app.quit();
        return GLib.SOURCE_REMOVE;
    });
});
app.run([]);
if (deadline)
    GLib.source_remove(deadline);
imports.system.exit(result);
