"""GDB backtrace capture for test failure reports."""

from __future__ import annotations

import subprocess
from pathlib import Path

_GDB_EXPAND_SCRIPT = str(Path(__file__).parent / "_gdb_mips_expand.py")


def capture_snapshot(
    elf: str,
    host: str = "localhost",
    port: int = 1234,
    timeout: float = 30.0,
) -> str:
    """Connect GDB to a running/halted QEMU, interrupt, dump full backtrace.

    Returns the combined stdout+stderr as a string. Never raises — on failure
    returns an error description so the caller can attach it to the test report.
    """
    if not Path(elf).is_file():
        return f"[gdb_snapshot] ELF not found: {elf}"

    cmd = [
        "gdb-multiarch", "-nx", "-batch",
        "-ex", "set architecture mips:isa32r2",
        "-ex", "set endian little",
        "-ex", f"file {elf}",
        "-ex", f"source {_GDB_EXPAND_SCRIPT}",
        "-ex", f"target remote {host}:{port}",
        "-ex", "interrupt",
        "-ex", "set pagination off",
        "-ex", "thread apply all backtrace full",
        "-ex", "disconnect",
        "-ex", "quit",
    ]
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return result.stdout + (("\n--- stderr ---\n" + result.stderr) if result.stderr.strip() else "")
    except subprocess.TimeoutExpired:
        return f"[gdb_snapshot] timed out after {timeout}s"
    except FileNotFoundError:
        return "[gdb_snapshot] gdb-multiarch not found — install gdb-multiarch"
