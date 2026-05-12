# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
# Install with optional extras
pip install -e ".[can,build]"
# or with Poetry
poetry install --with dev

# Run tests
pytest tests/ -v

# Run a single test
pytest tests/test_bootloader.py::TestBootloaderRuns::test_bootloader_runs -v

# Install package in development mode (auto-discovers plugin via pytest11 entry point)
pip install -e .
```

> **Note:** Tests require a custom `qemu-system-mipsel` build with `pic32mk` machine support. They will not run in CI without it. Use `Pic32mkConfig(build=False)` to skip the firmware build when pre-built artifacts exist.

### Consumer project debug commands (run inside the firmware project)

```bash
# One-shot: generate .vscode/launch.json + tasks.json for QEMU GDB debugging
pytest --qemu-vscode-init

# Start QEMU halted at reset, wait for VSCode/GDB to attach (Ctrl+C to stop)
pytest --qemu-start-debug
```

After `--qemu-vscode-init`, press F5 in VSCode → "QEMU Debug — \<project\_name\>" — VSCode starts QEMU automatically via the background task and attaches `gdb-multiarch`.

## Architecture

This is a **pytest plugin** (`pytest11` entry point → `pytest_qemu_pic32mk.plugin`) for functional testing of PIC32MK1024MCM100 (MIPS32r2) firmware running under QEMU emulation.

### Two build modes

- **Workspace mode** (`project_src_dir` set): Plugin creates `.pytest-qemu-build/`, symlinks foreign firmware sources as `TARGET/`, injects bundled `wrapper/`, copies `pymake/Makefile.py`, then drives `pymaketool all` + `make`. Zero setup needed in consumer projects.
- **Custom build mode** (`project_src_dir=None`): Runs `build_cmd` in `build_workspace`. Consumer project supplies its own `Makefile.py`. Auto-runs `pymaketool` first if `pymake/makefile.mk` is missing.

### Three binary artifacts QEMU needs

| File | QEMU flag | Address | Content |
|---|---|---|---|
| `*.boot.bin` | `-bios` | `0xBFC00000` | `.reset` stub (~152 B) |
| `*.app.bin` | `-global pic32mk-nvm.filename=` | `0x9D000000` | kseg0 (code, rodata) |
| `*.rw.bin` | `-device loader,…` | from `*.rw.addr` | initialized `.data` |

`*.rw.bin` is required because `crt0.S` only zeroes `.bss` — it has no LMA→VMA copy loop. QEMU must preload initialized globals directly into RAM.

### Two QEMU scopes

- **Session QEMU** (`qemu_proc` fixture): one boot per test session, shared by all non-`qemu_state` tests. Accessed via `gpio` and `qmp_client` fixtures.
- **Isolated QEMU** (`qemu` fixture, class-scoped): per-`@pytest.mark.qemu_state(...)` class. Killed after the last test in the class. Uses separate ports/sockets (`gdb_port_isolated`, `qmp_sock_isolated`, `bms_uart_sock_isolated`).

### Key source modules

| Module | Role |
|---|---|
| [plugin.py](src/pytest_qemu_pic32mk/plugin.py) | pytest fixtures, `qemu_state` marker, failure hook (GDB snapshot), startup DBE guard |
| [config.py](src/pytest_qemu_pic32mk/config.py) | `Pic32mkConfig` dataclass — all tunable settings |
| [qemu.py](src/pytest_qemu_pic32mk/qemu.py) | `QEMUProcess` — launch/kill QEMU, QMP ready-wait, vcan setup, stdout capture |
| [build.py](src/pytest_qemu_pic32mk/build.py) | `build_firmware` — dispatches workspace vs custom mode |
| [gpio.py](src/pytest_qemu_pic32mk/gpio.py) | `GPIOHelper` / `PinState` — QMP-based GPIO/ADC read-write |
| [gpio_tool.py](src/pytest_qemu_pic32mk/gpio_tool.py) | `QMPClient` — raw QMP socket protocol |
| [gdb_snapshot.py](src/pytest_qemu_pic32mk/gdb_snapshot.py) | `capture_snapshot` — GDB backtrace on failure |
| [bms_mock.py](src/pytest_qemu_pic32mk/bms_mock.py) | `BMSMock` — bq79606 UART emulator over Unix socket |
| [can_bus.py](src/pytest_qemu_pic32mk/can_bus.py) | `CANHelper` / `CANResponse` — SocketCAN send/receive |
| [build_utils.py](src/pytest_qemu_pic32mk/build_utils.py) | `extract_rw_segment`, `scan_elf`, `validate_objects_dir` |
| [vscode.py](src/pytest_qemu_pic32mk/vscode.py) | `generate_vscode_debug_config` — writes `.vscode/launch.json` + `tasks.json` for QEMU GDB debugging |

### Bundled MIPS wrapper (`src/pytest_qemu_pic32mk/wrapper/`)

Cross-compile glue files for QEMU emulation injected into every workspace build:
- `startup/crt0.S` — reset vector at `0xBFC00000`, CP0 init, BSS zero
- `startup/irq_dispatch.S` — interrupt dispatch table (single-vector mode)
- `stubs/` — libc stubs, FreeRTOS overrides, minimal `stdio.h`/`stdlib.h`
- `xc32/xc.h` — CP0 macros + MIPS intrinsics (XC32 `<xc.h>` compat)

### GDB injection

BMS struct fields are written into firmware RAM via `gdb-multiarch` at `main()` entry (`_inject_bms_via_gdb` in `qemu.py`). CPU must be halted (`-S` flag) when this runs; the helper sets a breakpoint at `main()`, resumes past `crt0`, writes values, then detaches.

### XC32 fmt=3 bug detector

`validate_objects_dir` / `scan_elf` detect the XC32 linker bug where uniform non-zero static arrays with `size % 4 != 0` trigger `__dinit_copy_val_data` word-loop corruption. The startup DBE guard (`_check_startup_dbe` fixture) catches this at runtime by matching `exception_code=0x7` + `EPC=0xBFC00000` in QEMU output.

## Consumer project integration

Foreign projects install this as a dev dependency and override only `pic32mk_config` in their `conftest.py`. The plugin is auto-discovered — no import needed. `wrapper/` files and `pymake/Makefile.py` are bundled in the installed package and referenced via `get_wrapper_dir()`.

Add `.pytest-qemu-build/` to `.gitignore` in consumer projects.
