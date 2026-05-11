#!/usr/bin/env python3
"""
gpio_tool.py — Host-side GPIO/ADC control for PIC32MK QEMU emulator via QMP/QOM.

Uses QEMU's built-in qom-get / qom-set commands over the QMP socket.
No extra dependencies beyond Python 3.6+ standard library.

QOM paths (registered as named children of the machine object):
  PORTA → /machine/gpio-portA
  ...
  PORTG → /machine/gpio-portG

Available QOM properties on each gpio-portX device:
  pin0 … pin15   bool  r/w  inject external input (w) / read PORT bit (r)
  lat-state       u32   r    current LAT register (output latch)
  port-state      u32   r    current PORT register (mixed in/out)
  tris-state      u32   r    current TRIS register (1=input, 0=output)

ADC QOM path: /machine/pic32mk-adc
  adc-ch<N>       int   r/w  inject 12-bit analog input for channel N
  adc-data<N>     int   r    last conversion result for channel N
"""

from __future__ import annotations

import json
import socket

PORT_NAMES = {
    'A': 'gpio-portA', 'B': 'gpio-portB', 'C': 'gpio-portC',
    'D': 'gpio-portD', 'E': 'gpio-portE', 'F': 'gpio-portF',
    'G': 'gpio-portG',
}

ADC_QOM_PATH = "/machine/pic32mk-adc"


class QMPClient:
    """Minimal QMP client over a UNIX or TCP socket."""

    def __init__(self, path: str):
        if path.startswith('tcp:'):
            _, host, port = path.split(':', 2)
            self._sock = socket.create_connection((host, int(port)))
        else:
            self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self._sock.connect(path)
        self._buf = b''
        # Read and discard the QMP greeting banner
        self._read_line()
        # Send capabilities negotiation
        self._send({'execute': 'qmp_capabilities'})
        self._read_response()

    def _send(self, obj: dict):
        data = json.dumps(obj).encode() + b'\n'
        self._sock.sendall(data)

    def _read_line(self) -> str:
        while b'\n' not in self._buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise ConnectionError('QMP socket closed')
            self._buf += chunk
        line, self._buf = self._buf.split(b'\n', 1)
        return line.decode()

    def _read_response(self) -> dict:
        """Read lines until we get a 'return' or 'error' key."""
        while True:
            line = self._read_line()
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            if 'return' in obj or 'error' in obj:
                return obj
            # 'event' lines — ignore

    def execute(self, cmd: str, **args) -> object:
        req = {'execute': cmd}
        if args:
            req['arguments'] = args
        self._send(req)
        resp = self._read_response()
        if 'error' in resp:
            raise RuntimeError(f"QMP error: {resp['error']}")
        return resp['return']

    def qom_get(self, path: str, prop: str) -> object:
        return self.execute('qom-get', path=path, property=prop)

    def qom_set(self, path: str, prop: str, value: object):
        self.execute('qom-set', path=path, property=prop, value=value)

    def qom_list(self, path: str) -> list:
        return self.execute('qom-list', path=path)

    def cont(self):
        """Resume a CPU that was started halted (-S)."""
        self.execute('cont')

    def close(self):
        self._sock.close()


def gpio_path(port_letter: str) -> str:
    name = PORT_NAMES.get(port_letter.upper())
    if name is None:
        raise ValueError(f"Unknown port '{port_letter}'. Use A–G.")
    return f'/machine/{name}'
