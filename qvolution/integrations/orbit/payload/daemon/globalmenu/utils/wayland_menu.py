"""Orbit's native-Wayland bridge for canonical dbusmenu exporters.

Connection credentials come from the bus daemon, not a client-supplied PID.
This is an Orbit extension to its daemon protocol, not the X11 registrar API.
"""
import re


class WaylandMenuRegistry:
    def __init__(self, credentials, uid):
        self.credentials = credentials
        self.uid = uid
        self.peers = {}

    def register(self, sender, path):
        if not re.fullmatch(r':\d+\.\d+', sender) or not re.fullmatch(r'/(?:[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)*)?', path):
            raise ValueError('Invalid menu endpoint')
        pid, uid = self.credentials(sender)
        if uid != self.uid or not isinstance(pid, int) or not 0 < pid <= 0xffffffff:
            raise ValueError('Menu peer must have this user ID and a valid process ID')
        self.peers[sender] = (pid, path)

    def remove(self, sender):
        self.peers.pop(sender, None)

    def lookup(self, pid):
        matches = [(sender, path) for sender, (peer_pid, path) in self.peers.items() if peer_pid == pid]
        if len(matches) != 1:
            raise LookupError('No unambiguous menu for this process')
        return matches[0]
