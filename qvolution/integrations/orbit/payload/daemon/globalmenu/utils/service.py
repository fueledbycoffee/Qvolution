import dbus
import os
import dbus.service

from gi.repository import GLib
from globalmenu.utils.wayland_menu import WaylandMenuRegistry

BUS_NAME = 'com.canonical.AppMenu.Registrar'
BUS_PATH = '/com/canonical/AppMenu/Registrar'


class AppMenuService(dbus.service.Object):
	def __init__(self):
		self.window_dict = dict()

		bus = dbus.SessionBus()
		bus.add_signal_receiver(self.name_owner_changed, 'NameOwnerChanged',
			'org.freedesktop.DBus', 'org.freedesktop.DBus', '/org/freedesktop/DBus')

		bus_name = dbus.service.BusName(BUS_NAME, bus=bus)
		dbus.service.Object.__init__(self, bus_name, BUS_PATH)

	def name_owner_changed(self, name, old_owner, new_owner):
		if not new_owner:
			for window_id, (sender, _) in list(self.window_dict.items()):
				if sender == name:
					del self.window_dict[window_id]

	@dbus.service.method(BUS_NAME, in_signature='uo', sender_keyword='sender')
	def RegisterWindow(self, windowId, menuObjectPath, sender):
		self.window_dict[windowId] = [dbus.String(sender), dbus.ObjectPath(menuObjectPath)]

	@dbus.service.method(BUS_NAME, in_signature='u')
	def UnregisterWindow(self, windowId):
		if windowId in self.window_dict:
			del self.window_dict[windowId]

	@dbus.service.method(BUS_NAME, in_signature='u', out_signature='so')
	def GetMenuForWindow(self, windowId):
		if windowId in self.window_dict:
			return self.window_dict[windowId]

	@dbus.service.method(BUS_NAME, out_signature='a{u(so)}')
	def GetMenus(self):
		return self.window_dict

	@dbus.service.method(BUS_NAME)
	def Q(self):
		GLib.MainLoop().quit()


class MyService(dbus.service.Object):

	BUS_PATH = '/space/unmade/GlobalMenu'
	BUS_NAME = 'space.unmade.GlobalMenu'

	def __init__(self):
		self._session = dbus.SessionBus()
		self.bus_name = dbus.service.BusName(self.BUS_NAME, bus=self._session)
		self._wayland_menus = WaylandMenuRegistry(self._menu_peer_credentials, os.getuid())
		self._wayland_owner_match = self._session.add_signal_receiver(
			self._menu_owner_changed, 'NameOwnerChanged', 'org.freedesktop.DBus',
			'org.freedesktop.DBus', '/org/freedesktop/DBus')
		dbus.service.Object.__init__(self, self.bus_name, self.BUS_PATH)

	def _menu_peer_credentials(self, sender):
		obj = self._session.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
		driver = dbus.Interface(obj, 'org.freedesktop.DBus')
		return (int(driver.GetConnectionUnixProcessID(sender, timeout=2)),
			int(driver.GetConnectionUnixUser(sender, timeout=2)))

	def _menu_owner_changed(self, name, old_owner, new_owner):
		if not new_owner:
			self._wayland_menus.remove(str(name))

	@dbus.service.method(BUS_NAME, in_signature='o', sender_keyword='sender')
	def RegisterWaylandMenu(self, menu_path, sender):
		try:
			self._wayland_menus.register(str(sender), str(menu_path))
		except ValueError as error:
			raise dbus.exceptions.DBusException(str(error), name=self.BUS_NAME + '.InvalidMenuPeer')

	@dbus.service.method(BUS_NAME, sender_keyword='sender')
	def UnregisterWaylandMenu(self, sender):
		self._wayland_menus.remove(str(sender))

	@dbus.service.method(BUS_NAME, in_signature='u', out_signature='so')
	def GetMenuForProcess(self, pid):
		try:
			name, path = self._wayland_menus.lookup(int(pid))
			return dbus.String(name), dbus.ObjectPath(path)
		except LookupError as error:
			raise dbus.exceptions.DBusException(str(error), name=self.BUS_NAME + '.MenuNotFound')

	@dbus.service.signal(BUS_NAME, signature='su')
	def MenuActivated(self, menu, x):
		pass

	@dbus.service.method(BUS_NAME, in_signature='su')
	def EchoSignal(self, menu, x):
		self.MenuActivated(menu, x)

	@dbus.service.method(BUS_NAME, in_signature='a{ss}')
	def WindowSwitched(self, window_data):
		self.WindowSwitchedSignal(window_data)

	@dbus.service.signal(BUS_NAME, signature='a{ss}')
	def WindowSwitchedSignal(self, window_data):
		pass

	@dbus.service.method(BUS_NAME, in_signature='as')
	def EchoSendTopLevelMenus(self, top_level_menus):
		self.SendTopLevelMenus(top_level_menus)

	# the tree carries its request id in its own signal
	# so SendMenuTree keeps its signature for old extensions
	@dbus.service.method(BUS_NAME, in_signature='ss')
	def EchoSendMenuTree(self, tree_json, request):
		self.MenuTreeRequest(request)
		self.SendMenuTree(tree_json)

	@dbus.service.signal(BUS_NAME, signature='s')
	def MenuTreeRequest(self, request):
		pass

	@dbus.service.signal(BUS_NAME, signature='s')
	def SendMenuTree(self, tree_json):
		pass

	@dbus.service.method(BUS_NAME, in_signature='s')
	def ActivateMenuItem(self, item_path):
		self.ActivateMenuItemSignal(item_path)

	@dbus.service.signal(BUS_NAME, signature='s')
	def ActivateMenuItemSignal(self, item_path):
		pass

	@dbus.service.method(BUS_NAME)
	def RequestMenuTree(self):
		self.RequestMenuTreeSignal()

	@dbus.service.signal(BUS_NAME)
	def RequestMenuTreeSignal(self):
		pass

	@dbus.service.signal(BUS_NAME, signature='as')
	def SendTopLevelMenus(self, top_level_menus):
		pass

	@dbus.service.method(BUS_NAME, in_signature='b')
	def EchoMenuOnOff(self, on):
		self.MenuOnOff(on)

	@dbus.service.signal(BUS_NAME, signature='b')
	def MenuOnOff(self, on):
		pass

	@dbus.service.method(BUS_NAME)
	def RequestWindowActions(self):
		self.RequestWindowActionsSignal()

	@dbus.service.signal(BUS_NAME)
	def RequestWindowActionsSignal(self):
		pass

	@dbus.service.method(BUS_NAME, in_signature='as')
	def ListWindowActions(self, action_list):
		self.ListWindowActionsSignal(action_list)

	@dbus.service.signal(BUS_NAME, signature='as')
	def ListWindowActionsSignal(self, top_level_menus):
		pass

	@dbus.service.method(BUS_NAME, in_signature='s')
	def ActivateWindowAction(self, action):
		self.ActivateWindowActionSignal(action)

	@dbus.service.signal(BUS_NAME, signature='s')
	def ActivateWindowActionSignal(self, action):
		pass

	@dbus.service.method(BUS_NAME)
	def EmitHudActivated(self):
		self.HudActivated()

	@dbus.service.signal(BUS_NAME)
	def HudActivated(self):
		pass
