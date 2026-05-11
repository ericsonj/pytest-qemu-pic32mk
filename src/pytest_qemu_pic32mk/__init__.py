"""pytest-qemu-pic32mk — pytest plugin and build utilities for PIC32MK QEMU emulation.

Public API
----------
* :class:`Pic32mkConfig`    — configuration dataclass for ``pic32mk_config`` fixture
* :class:`QEMUProcess`      — QEMU process lifecycle manager
* :class:`QMPClient`        — low-level QMP socket client
* :class:`GPIOHelper`       — GPIO / ADC read-write helper
* :class:`PinState`         — fluent GPIO state result
* :class:`Pin`              — pin descriptor Protocol (implement in your project)
* :func:`capture_snapshot`  — GDB backtrace on test failure
* :class:`BMSMock`          — bq79606 UART mock (write capture + register injection)
* :class:`WriteRecord`      — parsed BMS write record
* :class:`CANHelper`        — SocketCAN send/receive helper
* :class:`CANResponse`      — fluent CAN response wrapper
* :func:`get_wrapper_dir`   — returns the bundled ``wrapper/`` path

Build utilities (see :mod:`pytest_qemu_pic32mk.build_utils`):
* :func:`extract_rw_segment`  — ELF32 → QEMU RAM init binary
* :func:`scan_elf`            — XC32 fmt=3 violation scanner
* :func:`validate_objects_dir` — scan all .o files in a directory

Fixtures auto-loaded by pytest (do not import explicitly):
  pic32mk_config, pic32mk_initial_pins, pic32mk_initial_adc,
  qemu_proc, qemu, gpio, qmp_client, qemu_state (marker)
"""

from pathlib import Path

from .config import Pic32mkConfig
from .qemu import QEMUProcess
from .gpio_tool import QMPClient, gpio_path, ADC_QOM_PATH
from .gpio import GPIOHelper, PinState, Pin
from .gdb_snapshot import capture_snapshot
from .bms_mock import BMSMock, WriteRecord
from .can_bus import CANHelper, CANResponse
from .build import build_firmware
from .build_utils import extract_rw_segment, scan_elf, validate_objects_dir
from .plugin import QEMUBundle


def get_wrapper_dir() -> Path:
    """Return the absolute path to the bundled ``wrapper/`` directory.

    Use this in your project's ``Makefile.py`` / pymaketool configuration to
    reference the bundled MIPS wrapper files::

        from pytest_qemu_pic32mk import get_wrapper_dir
        WRAPPER = get_wrapper_dir()
        # → /path/to/site-packages/pytest_qemu_pic32mk/wrapper
    """
    return Path(__file__).parent / "wrapper"


__all__ = [
    # Config
    "Pic32mkConfig",
    # QEMU process
    "QEMUProcess",
    # QMP client
    "QMPClient",
    "gpio_path",
    "ADC_QOM_PATH",
    # GPIO helper
    "GPIOHelper",
    "PinState",
    "Pin",
    # GDB snapshot
    "capture_snapshot",
    # BMS mock
    "BMSMock",
    "WriteRecord",
    # CAN
    "CANHelper",
    "CANResponse",
    # Build
    "build_firmware",
    # Build utils
    "extract_rw_segment",
    "scan_elf",
    "validate_objects_dir",
    # Wrapper path
    "get_wrapper_dir",
    "QEMUBundle",
]
