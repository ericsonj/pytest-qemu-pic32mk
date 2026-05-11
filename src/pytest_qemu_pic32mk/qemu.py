"""QEMU process lifecycle for pytest fixtures."""

from __future__ import annotations

import os
import re
import signal
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Sequence, Tuple

from .gpio_tool import QMPClient, gpio_path, ADC_QOM_PATH
from .gdb_snapshot import _GDB_EXPAND_SCRIPT

# ── Stable defaults (override via Pic32mkConfig) ──────────────────────────────
DEFAULT_QEMU_BIN = "qemu-system-mipsel"
DEFAULT_QMP_SOCK = "/tmp/qemu-qmp.sock"
DEFAULT_GDB_PORT = 1234
DEFAULT_QMP_SOCK_ISOLATED = "/tmp/qemu-qmp-isolated.sock"
DEFAULT_GDB_PORT_ISOLATED = 1235
DEFAULT_BMS_UART_SOCK = "/tmp/qemu-bms-uart.sock"
DEFAULT_BMS_UART_SOCK_ISOLATED = "/tmp/qemu-bms-uart-isolated.sock"


def _ensure_vcan(iface: str) -> None:
    result = subprocess.run(["ip", "link", "show", "dev", iface],
                            capture_output=True)
    if result.returncode == 0:
        if b"UP" not in result.stdout:
            subprocess.run(["sudo", "ip", "link", "set", "dev", iface, "up"],
                           check=True)
        return
    subprocess.run(["sudo", "modprobe", "vcan"], check=True)
    subprocess.run(["sudo", "ip", "link", "add", "dev", iface, "type", "vcan"],
                   check=True)
    subprocess.run(["sudo", "ip", "link", "set", "dev", iface, "up"],
                   check=True)


def _kill_existing(qmp_sock: str) -> None:
    """Kill any existing QEMU process that owns *qmp_sock*."""
    result = subprocess.run(["ps", "-eo", "pid,args"], capture_output=True, text=True)
    for line in result.stdout.splitlines():
        if "qemu-system-mipsel" in line and qmp_sock in line:
            pid = int(line.split()[0])
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass


def _inject_bms_via_gdb(elf: str, port: int, bms_data: "dict[str, int]") -> None:
    """Write BMS info values into firmware RAM at ``main()`` entry via GDB.

    CPU must be halted (-S) when called. Sets a breakpoint at main(), resumes
    (runs past crt0 BSS zeroing), writes values, then detaches so the CPU
    continues running.
    """
    cmds = [
        "gdb-multiarch", "-nx", "-batch",
        "-ex", "set architecture mips:isa32r2",
        "-ex", "set endian little",
        "-ex", f"file {elf}",
        "-ex", f"source {_GDB_EXPAND_SCRIPT}",
        "-ex", f"target remote localhost:{port}",
        "-ex", "maintenance expand-symtabs",
        "-ex", "break main",
        "-ex", "continue",
    ]
    for field, value in bms_data.items():
        expr = f"'battery_monitoring.c'::bms_comm_data.bms_info[{field}]"
        cmds += ["-ex", f"set {expr} = {value}"]
        print(f"[QEMU] init BMS {field} = {value}", flush=True)
    cmds += ["-ex", "detach", "-ex", "quit"]
    result = subprocess.run(cmds, capture_output=True, text=True, timeout=30)
    print(f"[QEMU] BMS GDB inject output:\n{result.stdout}", flush=True)
    if result.returncode != 0 or result.stderr.strip():
        print(f"[QEMU] BMS GDB inject stderr:\n{result.stderr}", flush=True)


class QEMUStdio:
    """Thread-safe capture of QEMU stdout for test assertions."""

    def __init__(self) -> None:
        self._lines: list[str] = []
        self._cond = threading.Condition()

    def feed(self, line: str) -> None:
        with self._cond:
            self._lines.append(line)
            self._cond.notify_all()

    @property
    def lines(self) -> list[str]:
        """Snapshot copy of all lines received so far."""
        with self._cond:
            return list(self._lines)

    def contains(self, pattern: str) -> bool:
        """Return True if *pattern* appears as a substring in any captured line."""
        with self._cond:
            return any(pattern in line for line in self._lines)

    def wait_for(self, pattern: str, timeout: float = 10.0) -> str:
        """Block until *pattern* appears in any line; return the matched line.

        Raises ``TimeoutError`` if *pattern* is not seen within *timeout* seconds.
        """
        deadline = time.monotonic() + timeout
        with self._cond:
            while True:
                for line in self._lines:
                    if pattern in line:
                        return line
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(
                        f"Pattern {pattern!r} not seen in QEMU output within {timeout}s"
                    )
                self._cond.wait(timeout=remaining)


class QEMUProcess:
    """Running QEMU instance. Call ``kill()`` to stop."""

    _DBE_BEFORE_MAIN = re.compile(r"exception_code\s*=\s*0x00000007")
    _STALE_EPC = re.compile(r"exception_address\s*=\s*0xBFC00000")

    def __init__(
        self,
        proc: subprocess.Popen,
        bios: str,
        elf: str,
        gdb_port: int = DEFAULT_GDB_PORT,
        bms_uart_sock: str = DEFAULT_BMS_UART_SOCK,
    ):
        self._proc = proc
        self.bios = bios
        self.elf = elf
        self.gdb_port = gdb_port
        self.bms_uart_sock = bms_uart_sock
        self.pty_path: str | None = None
        self._pty_event = threading.Event()
        self._log_thread: threading.Thread | None = None
        self._output_lines: list[str] = []
        self._output_lock = threading.Lock()
        self.stdio = QEMUStdio()

    def _start_log_thread(self) -> None:
        assert self._proc.stdout is not None

        def _reader():
            assert self._proc.stdout is not None
            for line in self._proc.stdout:
                sys.stdout.write(f"[QEMU] {line}")
                sys.stdout.flush()
                with self._output_lock:
                    self._output_lines.append(line)
                self.stdio.feed(line)
                if self.pty_path is None:
                    m = re.search(r"char device redirected to (/dev/pts/\d+)", line)
                    if m:
                        self.pty_path = m.group(1)
                        self._pty_event.set()

        self._log_thread = threading.Thread(target=_reader, daemon=True)
        self._log_thread.start()

    def _wait_pty(self, timeout: float = 5.0) -> None:
        self._pty_event.wait(timeout=timeout)

    def _wait_qmp_ready(self, sock: str, timeout: float = 15.0) -> None:
        import socket as _socket
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                s = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM)
                s.connect(sock)
                s.close()
                return
            except (FileNotFoundError, ConnectionRefusedError):
                time.sleep(0.1)
        raise TimeoutError(f"QMP socket {sock!r} not ready after {timeout}s")

    def check_startup_dbe(self) -> str | None:
        """Return a diagnostic string if a pre-main Data Bus Error was detected."""
        with self._output_lock:
            text = "".join(self._output_lines)
        has_dbe = self._DBE_BEFORE_MAIN.search(text)
        has_stale_epc = self._STALE_EPC.search(text)
        if has_dbe and has_stale_epc:
            return (
                "pre-main Data Bus Error (exception_code=7, EPC=0xBFC00000) detected.\n"
                "On hardware this means __dinit_copy_val_data looped forever "
                "(array size % 4 != 0 with uniform non-zero init).\n"
                "Run: python3 validate_xc32_fmt3.py --objects-dir Release/Objects/"
            )
        return None

    @classmethod
    def start(
        cls,
        bios: str,
        program_flash: str,
        rw_bin: str,
        rw_addr: str,
        *,
        qemu_bin: str = DEFAULT_QEMU_BIN,
        run: bool = True,
        startup_delay: float = 1.0,
        initial_pins: Sequence[Tuple[str, int, bool]] = (),
        initial_adc: Sequence[Tuple[int, int]] = (),
        initial_bms: "dict[str, int] | None" = None,
        qmp_sock: str = DEFAULT_QMP_SOCK,
        gdb_port: int = DEFAULT_GDB_PORT,
        bms_uart_sock: str = DEFAULT_BMS_UART_SOCK,
        vcan_interfaces: Sequence[str] = (),
        extra_args: Sequence[str] = (),
        can_buses: "list[dict] | None" = None,
    ) -> "QEMUProcess":
        """Launch QEMU and wait until the QMP socket is ready.

        Args:
            bios:             Path to ``*.boot.bin`` (loaded at 0xBFC00000).
            program_flash:    Path to ``*.app.bin``  (loaded into NVM flash).
            rw_bin:           Path to ``*.rw.bin``   (preloaded RAM init image).
            rw_addr:          Physical address string (e.g. ``"0x80000010"``).
            qemu_bin:         Path to ``qemu-system-mipsel`` binary.
            run:              If ``True``, resume the CPU after setup; otherwise
                              leave it halted waiting for GDB.
            initial_pins:     GPIO pins to set before the CPU starts, each as
                              ``(port_letter, pin_number, value)``.
            initial_adc:      ADC channels to pre-load, each as
                              ``(channel, 12bit_value)``.
            initial_bms:      BMS ``bms_info`` dict injected via GDB at main().
            qmp_sock:         UNIX socket path for QMP.
            gdb_port:         TCP port for the GDB stub.
            bms_uart_sock:    UNIX socket path for UART1 (BMS mock).
            vcan_interfaces:  SocketCAN interfaces to create/bring-up.
            extra_args:       Extra raw arguments appended to QEMU command line.
            can_buses:        List of CAN bus dicts, each with keys:
                              ``{"bus_id": "canbus0", "iface": "vcan0"}``.
                              If ``None``, no CAN buses are configured.
        """
        for iface in vcan_interfaces:
            _ensure_vcan(iface)

        _kill_existing(qmp_sock)

        for _sock in (qmp_sock, bms_uart_sock):
            try:
                os.unlink(_sock)
            except FileNotFoundError:
                pass

        # Derive ELF path from bios: foo.boot.bin → foo.elf
        stem = Path(bios).name
        for suffix in (".boot.bin", ".app.bin", ".bin"):
            if stem.endswith(suffix):
                stem = stem[: -len(suffix)]
                break
        elf_path = str(Path(bios).parent / (stem + ".elf"))

        cmd = [
            qemu_bin, "-M", "pic32mk",
            "-icount", "shift=auto,sleep=on",
            "-semihosting",
            "-bios", bios,
            "-d", "unimp,guest_errors",
            "-qmp", f"unix:{qmp_sock},server=on,wait=off",
            "-nographic",
            "-chardev", f"socket,path={bms_uart_sock},server=on,wait=off,id=bms_uart_chardev",
            "-serial", "chardev:bms_uart_chardev",
            "-serial", "stdio",
            "-monitor", "none",
            "-chardev", "pty,id=usbcdc",
            "-device", f"loader,file={rw_bin},addr={rw_addr},force-raw=on",
            "-global", f"pic32mk-nvm.filename={program_flash}",
            "-gdb", f"tcp::{gdb_port}",
            "-S",
        ]

        if can_buses:
            for bus in can_buses:
                bid = bus["bus_id"]
                iface = bus["iface"]
                cmd += [
                    "-object", f"can-bus,id={bid}",
                    "-object", f"can-host-socketcan,id={bid.replace('bus', 'host')},if={iface},canbus={bid}",
                ]

        cmd += list(extra_args)

        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True,
                                bufsize=1)
        instance = cls(proc, bios, elf_path, gdb_port=gdb_port,
                       bms_uart_sock=bms_uart_sock)
        instance._start_log_thread()
        instance._wait_qmp_ready(sock=qmp_sock)
        instance._wait_pty()

        if initial_pins or initial_adc or initial_bms or run:
            boot_qmp = QMPClient(qmp_sock)
            try:
                for port, pin, value in initial_pins:
                    print(f"[QEMU] init PORT{port.upper()}.{pin} = {value}", flush=True)
                    boot_qmp.qom_set(gpio_path(port), f"pin{pin}", bool(value))
                for channel, value in initial_adc:
                    print(f"[QEMU] init ADC ch{channel} = {value}", flush=True)
                    boot_qmp.qom_set(ADC_QOM_PATH, f"adc-ch{channel}", value)
                if initial_bms and run:
                    _inject_bms_via_gdb(elf_path, gdb_port, initial_bms)
                if run:
                    boot_qmp.cont()
            finally:
                boot_qmp.close()

        if run:
            time.sleep(startup_delay)

        return instance

    def kill(self) -> None:
        """Terminate the QEMU process."""
        try:
            self._proc.terminate()
            self._proc.wait(timeout=5)
        except (subprocess.TimeoutExpired, OSError):
            self._proc.kill()
