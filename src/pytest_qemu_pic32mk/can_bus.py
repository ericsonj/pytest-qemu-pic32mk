"""CAN bus helper — thin wrapper around a send callable.

By default, if ``voltu-can-debugger`` is installed, this module provides a
``CANHelper`` that uses its transport layer.  You can also supply your own
``send_fn`` to decouple from the Voltu-specific transport.

Example with a custom transport::

    from pytest_qemu_pic32mk.can_bus import CANHelper

    def my_send(channel, addr, command, timeout_s):
        # return list of {"payload": str} dicts
        ...

    can = CANHelper("vcan0", idu_addr=3, send_fn=my_send)
"""

from __future__ import annotations

from typing import Callable, Optional

_voltu_available = False
try:
    from voltu_can_debugger.transport import send_command as _send_command
    from voltu_can_debugger.protocol import DEV_ADDR_IDU as _DEV_ADDR_IDU
    _voltu_available = True
except ImportError:
    _send_command = None  # type: ignore[assignment]
    _DEV_ADDR_IDU = 0x03


class CANResponse:
    """Fluent wrapper around a list of CAN frame result dicts.

    Each dict must have at least a ``"payload"`` key (str).
    Supports chained assertions (``.contains()`` / ``.not_contains()``) and is
    iterable / sized.
    """

    def __init__(self, frames: list[dict]) -> None:
        self._frames = frames

    def contains(self, text: str) -> "CANResponse":
        found = any(text in r["payload"] for r in self._frames)
        if not found:
            payloads = [r["payload"] for r in self._frames]
            raise AssertionError(
                f"Expected {text!r} in CAN response.\nGot: {payloads!r}"
            )
        return self

    def not_contains(self, text: str) -> "CANResponse":
        found = any(text in r["payload"] for r in self._frames)
        if found:
            payloads = [r["payload"] for r in self._frames]
            raise AssertionError(
                f"Expected {text!r} NOT in CAN response.\nGot: {payloads!r}"
            )
        return self

    def __iter__(self):
        return iter(self._frames)

    def __len__(self):
        return len(self._frames)

    def __bool__(self):
        return bool(self._frames)


class CANHelper:
    """Send debug commands over a SocketCAN interface and collect responses.

    Args:
        channel:     SocketCAN interface name (e.g. ``"vcan_dashboard"``).
        idu_addr:    Target device address on the CAN bus.
        send_fn:     Optional custom send callable with signature
                     ``(channel, addr, command, timeout_s) -> list[dict]``.
                     Each dict must contain at least ``{"payload": str}``.
                     If ``None``, the ``voltu-can-debugger`` transport is used.
    """

    def __init__(
        self,
        channel: str,
        idu_addr: int = _DEV_ADDR_IDU,
        send_fn: Optional[Callable] = None,
    ) -> None:
        self.channel = channel
        self.idu_addr = idu_addr

        if send_fn is not None:
            self._send_fn = send_fn
        elif _voltu_available:
            self._send_fn = _send_command
        else:
            def _missing(*_args, **_kwargs):
                raise ImportError(
                    "voltu-can-debugger is not installed and no send_fn was provided.\n"
                    "Install it or pass send_fn= to CANHelper."
                )
            self._send_fn = _missing

    def send(self, cmd_text: str, timeout_ms: int = 5000) -> CANResponse:
        """Send a debug command; return a :class:`CANResponse` of received frames."""
        return CANResponse(
            list(self._send_fn(self.channel, self.idu_addr, cmd_text, timeout_ms / 1000))
        )
