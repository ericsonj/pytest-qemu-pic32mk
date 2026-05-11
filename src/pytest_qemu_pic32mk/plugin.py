"""pytest plugin for pytest-qemu-pic32mk.

Provides session-scoped and class-scoped QEMU fixtures, a ``qemu_state``
marker, startup-crash guard, and automatic GDB backtrace on test failure.

────────────────────────────────────────────────────────────────────────────
Minimal project ``conftest.py``
────────────────────────────────────────────────────────────────────────────
::

    import pytest
    from pytest_qemu_pic32mk import Pic32mkConfig

    @pytest.fixture(scope="session")
    def pic32mk_config():
        return Pic32mkConfig(
            qemu_bin="/opt/qemu/build/qemu-system-mipsel",
            vcan_interfaces=["vcan_pwr_mgmt", "vcan_dashboard"],
        )

    # Optional: set initial GPIO/ADC state before the CPU starts
    @pytest.fixture(scope="session")
    def pic32mk_initial_pins():
        from myproject.signals import GPI_HVIL, HIGH
        return [(GPI_HVIL, HIGH)]          # list of (Pin, value) or (port, pin, bool)

    @pytest.fixture(scope="session")
    def pic32mk_initial_adc():
        return [(13, 2602)]                # list of (channel, 12bit_value)

────────────────────────────────────────────────────────────────────────────
qemu_state marker
────────────────────────────────────────────────────────────────────────────
::

    @pytest.mark.qemu_state(pins=[(GPI_HVIL, LOW)], adc=[(13, 0)])
    class TestHvilOpen:
        def test_fault_raised(self, qemu): ...

All test methods in the same class share one QEMU boot cycle (class scope).
A marker on a standalone test function creates a per-test QEMU instance.

────────────────────────────────────────────────────────────────────────────
Fixtures provided
────────────────────────────────────────────────────────────────────────────
* ``pic32mk_config``        (session)  — :class:`Pic32mkConfig` override point
* ``pic32mk_initial_pins``  (session)  — initial GPIO state, override point
* ``pic32mk_initial_adc``   (session)  — initial ADC state,  override point
* ``qemu_proc``             (session)  — long-running QEMU (one per session)
* ``qemu``                  (class)    — isolated QEMU per test class
* ``gpio``                  (session)  — :class:`GPIOHelper` on session QEMU
* ``qmp_client``            (session)  — raw :class:`QMPClient` on session QEMU
"""

from __future__ import annotations

import time
from dataclasses import dataclass
from pathlib import Path
from typing import Generator

import pytest

from .build import build_firmware
from .config import Pic32mkConfig
from .gpio import GPIOHelper
from .gpio_tool import QMPClient
from .gdb_snapshot import capture_snapshot
from .qemu import QEMUProcess, QEMUStdio


# ── Helpers ───────────────────────────────────────────────────────────────────

def _resolve_pin_tuple(entry) -> tuple[str, int, bool]:
    """Accept ``(Pin, value)`` or legacy ``(port_str, pin_int, bool)`` tuples."""
    if len(entry) == 2:
        pin, value = entry
        return (pin.port, pin.number, bool(value))
    return (entry[0], entry[1], bool(entry[2]))


def _find_release_files(config: Pic32mkConfig, rootdir: Path) -> dict:
    """Glob the release directory for the four required binary artifacts."""
    release = Path(config.release_dir)
    if not release.is_absolute():
        release = rootdir / release
    if not release.is_dir():
        pytest.fail(
            f"Release directory not found at {release}. "
            "Run 'make all' first or set pic32mk_config.release_dir."
        )

    def one(pattern: str) -> str:
        matches = sorted(release.glob(pattern))
        if not matches:
            pytest.fail(f"No file matching {release}/{pattern}. Run 'make all' first.")
        return str(matches[0])

    rw_addr_file = one("*.rw.addr")
    rw_addr = Path(rw_addr_file).read_text().strip()

    return {
        "bios":          one("*.boot.bin"),
        "program_flash": one("*.app.bin"),
        "rw_bin":        one("*.rw.bin"),
        "rw_addr":       rw_addr,
    }


# ── Plugin registration ───────────────────────────────────────────────────────

def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line(
        "markers",
        "qemu_state(pins=[], adc=[], bms={}, bms_uart={}): "
        "override boot-time GPIO pins, ADC values, BMS info (via GDB), or "
        "start a UART-level BMS mock for the isolated QEMU instance. "
        "All tests in the same class share one QEMU boot cycle.",
    )


# ── Config & initial-state overrides ─────────────────────────────────────────

@pytest.fixture(scope="session")
def pic32mk_config() -> Pic32mkConfig:
    """Base QEMU configuration.

    Override in your project ``conftest.py``::

        @pytest.fixture(scope="session")
        def pic32mk_config():
            return Pic32mkConfig(qemu_bin="/opt/qemu/build/qemu-system-mipsel")
    """
    return Pic32mkConfig()


@pytest.fixture(scope="session", autouse=True)
def _pic32mk_build(
    pic32mk_config: Pic32mkConfig,
    request: pytest.FixtureRequest,
) -> None:
    """Compile the firmware once per session, before any QEMU instance starts.

    Depends on ``pic32mk_config`` so the project's overridden config (with the
    correct ``build_dir``, ``build_cmd``, etc.) is used automatically.

    Build output streams live to the terminal.  A non-zero compiler exit code
    calls ``pytest.fail``, aborting the session before any test runs.

    Skip the build step::

        Pic32mkConfig(build=False)   # use pre-built artifacts in release_dir
    """
    build_firmware(pic32mk_config, Path(str(request.config.rootdir)))


@pytest.fixture(scope="session")
def pic32mk_initial_pins() -> list:
    """Initial GPIO pin state injected before the CPU executes its first instruction.

    Override in your project ``conftest.py``::

        @pytest.fixture(scope="session")
        def pic32mk_initial_pins():
            from myproject.signals import HVIL, HIGH
            return [(HVIL, HIGH)]
    """
    return []


@pytest.fixture(scope="session")
def pic32mk_initial_adc() -> list:
    """Initial ADC channel values injected before the CPU starts.

    Each entry: ``(channel_number, 12bit_value)``.

    Override in your project ``conftest.py``::

        @pytest.fixture(scope="session")
        def pic32mk_initial_adc():
            return [(13, 2602)]  # channel 13 ≈ 690 V
    """
    return []


# ── Session QEMU ─────────────────────────────────────────────────────────────

@pytest.fixture(scope="session")
def qemu_proc(
    pic32mk_config: Pic32mkConfig,
    pic32mk_initial_pins: list,
    pic32mk_initial_adc: list,
    _pic32mk_build: None,
    request: pytest.FixtureRequest,
) -> Generator[QEMUProcess, None, None]:
    """Long-running QEMU instance shared by all session-scoped tests."""
    files = _find_release_files(pic32mk_config, Path(request.config.rootdir))
    pins = [_resolve_pin_tuple(p) for p in pic32mk_initial_pins]
    adc  = list(pic32mk_initial_adc)

    proc = QEMUProcess.start(
        **files,
        qemu_bin=pic32mk_config.qemu_bin,
        run=True,
        initial_pins=pins,
        initial_adc=adc,
        qmp_sock=pic32mk_config.qmp_sock,
        gdb_port=pic32mk_config.gdb_port,
        bms_uart_sock=pic32mk_config.bms_uart_sock,
        vcan_interfaces=pic32mk_config.vcan_interfaces,
        extra_args=pic32mk_config.extra_qemu_args,
    )
    yield proc
    proc.kill()


@pytest.fixture(scope="session")
def qmp_client(
    pic32mk_config: Pic32mkConfig,
    qemu_proc: QEMUProcess,
    _stash_qemu: None,
) -> Generator[QMPClient, None, None]:
    """Raw QMP client connected to the session QEMU instance."""
    client = QMPClient(pic32mk_config.qmp_sock)
    yield client
    client.close()


@pytest.fixture(scope="session")
def gpio(qmp_client: QMPClient) -> GPIOHelper:
    """GPIO/ADC helper connected to the session QEMU instance."""
    return GPIOHelper(qmp_client)


# ── Class-scoped isolated QEMU ────────────────────────────────────────────────

@dataclass
class QEMUBundle:
    """Resources for a per-class QEMU instance (``qemu`` fixture)."""
    gpio: GPIOHelper
    _proc: QEMUProcess
    _qmp: QMPClient
    bms_mock: object = None  # BMSMock | None — typed as object to avoid hard dependency

    @property
    def stdio(self) -> QEMUStdio:
        """QEMU stdout capture for this isolated instance."""
        return self._proc.stdio


@pytest.fixture(scope="class")
def qemu(
    pic32mk_config: Pic32mkConfig,
    pic32mk_initial_pins: list,
    pic32mk_initial_adc: list,
    _pic32mk_build: None,
    request: pytest.FixtureRequest,
) -> Generator[QEMUBundle, None, None]:
    """Isolated QEMU instance per test class with custom boot state.

    Scope is *class* — all test methods in a class share one QEMU boot cycle.
    For a standalone test function, class scope behaves like function scope.

    Usage::

        @pytest.mark.qemu_state(pins=[(HVIL, LOW)], adc=[(13, 0)])
        class TestHvilOpen:
            def test_fault_set(self, qemu): ...
            def test_no_run(self, qemu): ...

        # Single test:
        @pytest.mark.qemu_state(pins=[(HVIL, LOW)])
        def test_hvil_open(qemu): ...
    """
    marker = request.node.get_closest_marker("qemu_state")
    default_pins = [_resolve_pin_tuple(p) for p in pic32mk_initial_pins]
    default_adc  = list(pic32mk_initial_adc)

    pins     = [_resolve_pin_tuple(p) for p in marker.kwargs.get("pins", default_pins)] if marker else default_pins
    adc      = list(marker.kwargs.get("adc",      default_adc))  if marker else default_adc
    bms      = dict(marker.kwargs.get("bms",      {}))           if marker else {}
    bms_uart = dict(marker.kwargs.get("bms_uart", {}))           if marker else {}

    files = _find_release_files(pic32mk_config, Path(request.config.rootdir))
    mock = None

    if bms_uart:
        # Import lazily to avoid hard dependency when BMS mock not used
        from .bms_mock import BMSMock
        proc = QEMUProcess.start(
            **files,
            qemu_bin=pic32mk_config.qemu_bin,
            run=False,
            initial_pins=pins,
            initial_adc=adc,
            qmp_sock=pic32mk_config.qmp_sock_isolated,
            gdb_port=pic32mk_config.gdb_port_isolated,
            bms_uart_sock=pic32mk_config.bms_uart_sock_isolated,
            vcan_interfaces=pic32mk_config.vcan_interfaces,
            extra_args=pic32mk_config.extra_qemu_args,
        )
        mock = BMSMock(pic32mk_config.bms_uart_sock_isolated, **bms_uart)
        mock.start()
        boot_qmp = QMPClient(pic32mk_config.qmp_sock_isolated)
        boot_qmp.cont()
        boot_qmp.close()
        print("[QEMU] BMS mock active, CPU resumed — waiting for firmware init...", flush=True)
        time.sleep(5.0)
    else:
        proc = QEMUProcess.start(
            **files,
            qemu_bin=pic32mk_config.qemu_bin,
            run=True,
            initial_pins=pins,
            initial_adc=adc,
            initial_bms=bms or None,
            qmp_sock=pic32mk_config.qmp_sock_isolated,
            gdb_port=pic32mk_config.gdb_port_isolated,
            bms_uart_sock=pic32mk_config.bms_uart_sock_isolated,
            vcan_interfaces=pic32mk_config.vcan_interfaces,
            extra_args=pic32mk_config.extra_qemu_args,
        )

    qmp = QMPClient(pic32mk_config.qmp_sock_isolated)
    bundle = QEMUBundle(
        gpio=GPIOHelper(qmp),
        _proc=proc,
        _qmp=qmp,
        bms_mock=mock,
    )
    yield bundle
    qmp.close()
    if mock:
        mock.stop()
    proc.kill()


# ── Pre-main DBE guard ────────────────────────────────────────────────────────

@pytest.fixture(autouse=True)
def _check_startup_dbe(request: pytest.FixtureRequest) -> Generator[None, None, None]:
    """Fail immediately if QEMU printed a Data Bus Error before ``main()``."""
    yield

    funcargs = request.node.funcargs if hasattr(request.node, "funcargs") else {}
    qemu_bundle = funcargs.get("qemu")
    qemu_session = funcargs.get("qemu_proc")
    proc: QEMUProcess | None = (
        qemu_bundle._proc if qemu_bundle is not None else qemu_session
    )
    if proc is not None:
        msg = proc.check_startup_dbe()
        if msg:
            pytest.fail(f"[startup crash] {msg}")


# ── Failure hook — auto GDB backtrace ────────────────────────────────────────

@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo):  # type: ignore[type-arg]
    outcome = yield
    rep = outcome.get_result()

    if rep.when != "call" or not rep.failed:
        return

    proc: QEMUProcess | None = item.session.__dict__.get("_qemu_proc_instance")
    if proc is None:
        try:
            proc = item.funcargs.get("qemu_proc")  # type: ignore[assignment]
        except AttributeError:
            pass

    if proc is None:
        bundle = item.funcargs.get("qemu")  # type: ignore[assignment]
        if bundle is not None:
            proc = bundle._proc

    if proc is not None and proc.elf:
        snapshot = capture_snapshot(proc.elf, port=proc.gdb_port)
        rep.sections.append(("GDB Snapshot", snapshot))


@pytest.fixture(scope="session")
def _stash_qemu(
    request: pytest.FixtureRequest,
    qemu_proc: QEMUProcess,
) -> Generator[None, None, None]:
    """Store QEMUProcess on the session so the failure hook can find it."""
    request.session.__dict__["_qemu_proc_instance"] = qemu_proc
    yield
