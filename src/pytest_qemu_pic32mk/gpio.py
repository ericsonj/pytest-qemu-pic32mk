"""GPIO and ADC helpers — thin wrapper around QMPClient."""

from __future__ import annotations

from typing import TYPE_CHECKING, Protocol, runtime_checkable

from .gpio_tool import QMPClient, gpio_path, ADC_QOM_PATH

if TYPE_CHECKING:
    pass


@runtime_checkable
class Pin(Protocol):
    """Duck-type for a named GPIO pin.  Projects define their own ``Pin`` enum/dataclass
    that satisfies this protocol — the library does not impose a specific type.

    Example::

        from dataclasses import dataclass

        @dataclass(frozen=True)
        class Pin:
            port: str    # "A" … "G"
            number: int  # 0-15
            name: str = ""
    """
    port: str
    number: int


class PinState:
    """Fluent result of ``GPIOHelper.get(pin)``.

    Raises ``AssertionError`` on mismatch via ``.is_high()`` / ``.is_low()``.
    """

    def __init__(self, pin: object, value: bool) -> None:
        self._pin = pin
        self._value = value

    def is_high(self) -> bool:
        if not self._value:
            raise AssertionError(f"{self._pin} expected HIGH, got LOW")
        return True

    def is_low(self) -> bool:
        if self._value:
            raise AssertionError(f"{self._pin} expected LOW, got HIGH")
        return True

    def __bool__(self) -> bool:
        return self._value


class GPIOHelper:
    """Read and write GPIO pins and ADC channels via QMP/QOM."""

    def __init__(self, qmp: QMPClient) -> None:
        self._qmp = qmp

    # ------------------------------------------------------------------
    # Named-signal API (preferred when using Pin objects)
    # ------------------------------------------------------------------

    def get(self, pin: Pin) -> PinState:
        return PinState(pin, self.get_pin(pin.port, pin.number))

    def set(self, pin: Pin, value: bool) -> None:
        self.set_pin(pin.port, pin.number, bool(value))

    # ------------------------------------------------------------------
    # Raw port/pin API
    # ------------------------------------------------------------------

    def set_pin(self, port: str, pin: int, value: bool) -> None:
        self._qmp.qom_set(gpio_path(port), f"pin{pin}", value)

    def get_pin(self, port: str, pin: int) -> bool:
        return bool(self._qmp.qom_get(gpio_path(port), f"pin{pin}"))

    def get_port_state(self, port: str) -> dict:
        """Return dict with keys ``tris``, ``lat``, ``port`` (all int)."""
        path = gpio_path(port)
        return {
            "tris": int(self._qmp.qom_get(path, "tris-state")),
            "lat":  int(self._qmp.qom_get(path, "lat-state")),
            "port": int(self._qmp.qom_get(path, "port-state")),
        }

    # ------------------------------------------------------------------
    # ADC helpers
    # ------------------------------------------------------------------

    def adc_set(self, channel: int, value: int) -> None:
        """Inject a 12-bit ADC value (0–4095) for *channel*."""
        self._qmp.qom_set(ADC_QOM_PATH, f"adc-ch{channel}", value)

    def adc_get(self, channel: int) -> int:
        """Read last conversion result for *channel*."""
        return int(self._qmp.qom_get(ADC_QOM_PATH, f"adc-data{channel}"))
