// Window metadata for Orbit's real exported-menu reader. Native Wayland uses
// the compositor's client PID; it never invents an X11 window ID.
export function nativeWindowData(win, appName, isX11) {
    const data = {xid: '0'};
    if (isX11) {
        const xid = win.get_description()?.match(/^0x[0-9a-f]+/);
        if (xid) data.xid = String(parseInt(xid[0]));
    } else {
        const pid = win.get_pid();
        if (Number.isInteger(pid) && pid > 0 && pid <= 0xffffffff) data.pid = String(pid);
    }
    for (const prop of ['gtk_unique_bus_name', 'gtk_application_id', 'gtk_application_object_path',
        'gtk_window_object_path', 'gtk_app_menu_object_path', 'gtk_menubar_object_path']) {
        if (win[prop] != null) data[prop] = win[prop];
    }
    if (appName) data.appName = appName;
    return data;
}
