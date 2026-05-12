# pytest-qemu-pic32mk

A pytest plugin and build-utility library for running firmware functional tests
against a QEMU-emulated **PIC32MK1024MCM100** (MIPS32r2 little-endian).

The library bundles:

- **pytest fixtures** — session and class-scoped QEMU lifecycle, GPIO/ADC injection,
  BMS UART mock, GDB snapshot on failure.
- **`qemu_state` marker** — declarative boot-time hardware state per test class.
- **MIPS wrapper C/ASM files** — the `crt0.S`, `irq_dispatch.S`, XC32 compat headers, and
  linker stubs that any foreign firmware project needs to cross-compile for QEMU.
- **Build utilities** — `extract_rw_segment` (ELF → QEMU RAM init) and `scan_elf`
  (XC32 fmt=3 bug detector).

The QEMU binary must be a custom build with `pic32mk` machine support.

---

## Requirements

| Requirement | Version |
|---|---|
| Python | ≥ 3.10 |
| pytest | ≥ 7.0 |
| qemu-system-mipsel | custom `pic32mk` build |
| gdb-multiarch | for GDB snapshot on failure + VSCode debugging |
| SocketCAN kernel modules | for CAN tests (`vcan`) |

Optional — CAN support:

```
pip install pytest-qemu-pic32mk[can]
```

---

## Installation

### In a foreign firmware project

```toml
# pyproject.toml (Poetry)
[tool.poetry.dependencies]
pytest-qemu-pic32mk = { path = "/path/to/pytest-qemu-pic32mk", develop = true }
```

Or from a package index once published:

```
pip install pytest-qemu-pic32mk
```

The plugin is auto-discovered by pytest via the `pytest11` entry point — no
`conftest.py` import needed.

---

## Quick start

### Workspace mode

The plugin creates an isolated build workspace, symlinks your firmware sources
and the bundled MIPS wrapper, and drives `pymaketool` + `make` automatically —
no `Makefile` or pymaketool configuration needed in your firmware project.
**Firmware sources are never copied — only symlinked.**

Create a minimal `conftest.py` that points to your firmware sources:

```python
# tests/conftest.py
import pytest
from pathlib import Path
from pytest_qemu_pic32mk import Pic32mkConfig

@pytest.fixture(scope="session")
def pic32mk_config():
    return Pic32mkConfig(
        qemu_bin="/opt/qemu-pic32mk/build/qemu-system-mipsel",
        # Path to the firmware project root (where your .ld, FreeRTOS config live).
        # The plugin creates .pytest-qemu-build/, symlinks your sources as TARGET/,
        # injects the bundled wrapper/, and runs pymaketool + make.
        project_src_dir=Path(__file__).parent.parent,  # one level up
        project_name="MY-FIRMWARE",  # output prefix for *.boot.bin etc.
        # Optional: path to your linker script (relative to project_src_dir).
        # Defaults to "firmware/src/config/default/p32MK1024MCM100.ld"
        linker_script="src/config/default/p32MK1024MCM100.ld",
        # SocketCAN interfaces — creates vcan devices and wires QEMU CAN buses.
        # List form: sequential from CAN1 (index 0 = CAN1, index 1 = CAN2, …)
        vcan_interfaces=["vcan_pwr_mgmt", "vcan_dashboard"],
        # Dict form: explicit 1-indexed firmware CANFD port numbers
        # vcan_interfaces={3: "vcan_pwr_mgmt"},   # CAN3 only → QEMU canbus2
    )
```

**That's it!**  When you run `pytest`:
1. The plugin creates `.pytest-qemu-build/` workspace
2. Copies bundled `Makefile.py` + helper scripts into `pymake/`
3. Symlinks your firmware sources as `TARGET/` (no duplication)
4. Runs `pymaketool all` then `make -f pymake/makefile.mk all` (streaming output)
5. QEMU launches with the fresh `Release/*.boot.bin`, `*.app.bin`, `*.rw.bin`
6. Tests run

To skip the build and use already-built QEMU artifacts (e.g. in CI with a pre-built cache):

```python
Pic32mkConfig(build=False, release_dir=".pytest-qemu-build/Release")
```

### Define your pin descriptors (`signals.py`)

The `Pin` protocol requires only `.port: str` and `.number: int`.
Use a dataclass, NamedTuple, or any object with those two fields:

```python
# tests/signals.py
from dataclasses import dataclass

@dataclass(frozen=True)
class Pin:
    port: str
    number: int

# GPIO inputs
GPI_HVIL    = Pin("B", 6)   # High-Voltage Interlock
GPI_KEY_RUN = Pin("C", 4)

# ADC channels
IEP_VOLTAGE = 13   # ADCHS_CH13 — pack voltage

# Logic levels
HIGH = True
LOW  = False

def V(volts: float) -> int:
    """Convert volts → 12-bit ADC counts (3.3 V reference, 68k/10k divider)."""
    return int(volts * 4095 / 3.3 * (10 / 78))
```

### Set boot-time hardware state

Override `pic32mk_initial_pins` and `pic32mk_initial_adc` for the default state
shared by the session QEMU instance (no `qemu_state` marker):

```python
# tests/conftest.py  (continued)
from tests.signals import GPI_HVIL, HIGH, IEP_VOLTAGE, V

@pytest.fixture(scope="session")
def pic32mk_initial_pins():
    return [(GPI_HVIL, HIGH)]          # HVIL closed → normal operation

@pytest.fixture(scope="session")
def pic32mk_initial_adc():
    return [(IEP_VOLTAGE, V(690))]     # nominal 690 V pack
```

---

## VSCode debugging

The plugin can generate a VSCode debug configuration that lets you attach
`gdb-multiarch` to the running QEMU instance with one click (F5).

### One-time setup

Run this once in your firmware project root:

```bash
poetry run pytest --qemu-vscode-init
```

This builds the firmware and writes/updates two files:

- `.vscode/launch.json` — adds a `"QEMU Debug — <project_name>"` attach configuration
- `.vscode/tasks.json` — adds a `"QEMU: Start for debugging"` background task

Existing entries (e.g. MPLAB/PICkit5 configs) are preserved.

### Debug workflow (F5)

Press **F5** in VSCode → select **"QEMU Debug — \<project\_name\>"**:

1. VSCode runs the background task: `poetry run pytest --qemu-start-debug`
2. The task builds firmware (if needed) and starts QEMU with the CPU **halted at reset**
3. When QEMU prints `GDB stub : localhost:1234`, VSCode considers the task ready
4. `gdb-multiarch` attaches to `localhost:1234`, sources `_gdb_mips_expand.py` (MIPS symbol expander), and resumes the CPU
5. Firmware boots — set breakpoints in your `firmware/src/…` files

To stop: press the stop button in VSCode, then **Ctrl+C** in the QEMU task terminal.

### Manual QEMU start (without F5 auto-task)

You can also start QEMU manually and attach separately:

```bash
# Terminal A — start QEMU halted, wait for debugger
poetry run pytest --qemu-start-debug
# → [pytest-qemu-pic32mk] QEMU running — CPU halted at reset.
# →   GDB stub : localhost:1234
# →   Press Ctrl+C to stop QEMU.
```

Then attach in VSCode (Run & Debug → F5) or via CLI:

```bash
# Terminal B — attach gdb-multiarch
gdb-multiarch \
  -ex "set architecture mips:isa32r2" \
  -ex "set endian little" \
  -ex "file .pytest-qemu-build/Release/MY-FIRMWARE.elf" \
  -ex "target remote localhost:1234"
```

### IntelliSense (clangd) for QEMU build

After the first build, a `compile_commands.json` symlink is created at:

```
.pytest-qemu-build/compile_commands.json
```

Point clangd to the workspace directory to get IntelliSense with the correct
`mipsel-linux-gnu-gcc` flags (instead of XC32):

```yaml
# .clangd  (add to your firmware project root)
CompilationDatabase: .pytest-qemu-build
```

> **Note:** This replaces the XC32 IntelliSense source. If you need both,
> use separate clangd configurations or VS Code workspace settings to switch.

### Generated VSCode config (reference)

`launch.json` entry:
```json
{
  "name": "QEMU Debug — MY-FIRMWARE",
  "type": "cppdbg",
  "request": "attach",
  "program": "${workspaceFolder}/.pytest-qemu-build/Release/MY-FIRMWARE.elf",
  "miDebuggerPath": "gdb-multiarch",
  "miDebuggerServerAddress": "localhost:1234",
  "MIMode": "gdb",
  "setupCommands": [
    {"text": "set architecture mips:isa32r2", "ignoreFailures": false},
    {"text": "set endian little",              "ignoreFailures": false},
    {"text": "set remotetimeout 30",           "ignoreFailures": false},
    {"text": "source /path/to/_gdb_mips_expand.py", "ignoreFailures": false}
  ],
  "sourceFileMap": {
    "/abs/path/.pytest-qemu-build/TARGET": "/abs/path/to/firmware"
  },
  "preLaunchTask": "QEMU: Start for debugging",
  "cwd": "${workspaceFolder}"
}
```

`tasks.json` entry:
```json
{
  "label": "QEMU: Start for debugging",
  "type": "shell",
  "command": "poetry run pytest --qemu-start-debug",
  "isBackground": true,
  "problemMatcher": {
    "pattern": {"regexp": "^NEVER$"},
    "background": {
      "activeOnStart": true,
      "beginsPattern": "\\[pytest-qemu-pic32mk\\] Building firmware",
      "endsPattern": "GDB stub\\s*:"
    }
  }
}
```

---

## Writing tests

### Session-scoped tests (simple, fast)

Use the `gpio` fixture for quick pin-toggle checks.
All tests in the session share one QEMU boot cycle.

```python
# tests/test_digital_inputs.py
import time
import pytest
from tests.signals import GPI_KEY_RUN, HIGH, LOW

def test_key_run_active(gpio):
    """Toggling GPI_KEY_RUN is reflected in firmware state."""
    gpio.set(GPI_KEY_RUN, LOW)
    time.sleep(0.05)
    assert gpio.get(GPI_KEY_RUN).is_low()
```

### Class-scoped isolated QEMU with `qemu_state`

Each class decorated with `@pytest.mark.qemu_state(...)` boots its own QEMU
instance with the declared GPIO/ADC state.  All methods in the class share that
boot cycle — QEMU is killed once the last method finishes.

```python
# tests/test_hvil.py
import time
import pytest
from pytest_qemu_pic32mk import QEMUBundle
from tests.signals import GPI_HVIL, IEP_VOLTAGE, LOW, V

@pytest.mark.qemu_state(
    pins=[(GPI_HVIL, LOW)],            # HVIL open at boot
    adc=[(IEP_VOLTAGE, V(690))],
)
class TestHvilOpen:
    """Firmware must raise FAULT_HVIL_OPEN and block Run when HVIL is open."""

    def test_fault_is_set(self, qemu: QEMUBundle):
        time.sleep(1.0)   # let the firmware initialise
        response = qemu.can_dash.send("GetFaults")
        response.contains("FAULT_HVIL_OPEN")

    def test_run_is_blocked(self, qemu: QEMUBundle):
        qemu.can_dash.send("PowerManagementRequest Run").contains("Failed")
```

### Voltage threshold tests with BMS UART mock

When `bms_uart=` is present in the marker, the library starts a
`BMSMock` (bq79606 UART emulator) before resuming the CPU.  This exercises
the real UART driver path rather than GDB variable injection.

```python
# tests/test_voltage.py
import time
import pytest
from tests.signals import GPI_HVIL, IEP_VOLTAGE, LOW, V

_CELL_LOW_MV  = 2500   # below LOW_CHARGE threshold (2876 mV)
_PACK_VOLTAGE = V(16 * 6 * _CELL_LOW_MV / 1000)

@pytest.mark.qemu_state(
    pins=[(GPI_HVIL, LOW)],
    adc=[(IEP_VOLTAGE, _PACK_VOLTAGE)],
    bms_uart={
        "n_devices": 16,
        "cell_voltage_mv": _CELL_LOW_MV,
    },
)
class TestVoltageLowFaultGate:
    """Cells at 2500 mV → FAULT_LOW_CELL_V set → Run transition blocked."""

    def test_low_voltage_blocks_run(self, qemu):
        time.sleep(5.0)   # BMS mock cycles; firmware sets the fault
        qemu.can_dash.send("PowerManagementRequest Run").contains("Failed")
```

### Direct GPIO read/write

```python
# tests/test_gpio.py
import pytest
from tests.signals import GPI_BRAKE_NO, GPI_KEY_RUN, HIGH, LOW

@pytest.mark.qemu_state(pins=[(GPI_BRAKE_NO, HIGH), (GPI_KEY_RUN, HIGH)])
class TestInputs:

    def test_brake_active(self, qemu):
        qemu.gpio.set(GPI_BRAKE_NO, LOW)
        qemu.can_dash.send("GetDigitalInput").contains("GPI_BRAKE_NO: Active")

    def test_key_run_released(self, qemu):
        qemu.gpio.set(GPI_KEY_RUN, HIGH)
        qemu.can_dash.send("GetDigitalInput").contains("GPI_KEY_RUN: Inactive")
```

### CAN bus helpers

```python
# tests/conftest.py  (continued)
from pytest_qemu_pic32mk import CANHelper, QEMUBundle

@pytest.fixture(scope="class")
def can_dash(qemu: QEMUBundle):
    return CANHelper("vcan_dashboard", idu_addr=0x03)

@pytest.fixture(scope="class")
def can_pwr(qemu: QEMUBundle):
    return CANHelper("vcan_pwr_mgmt", idu_addr=0x03)
```

Then in tests:

```python
def test_state_machine(can_dash):
    can_dash.send("PowerManagementRequest Idle").contains("State changed successfully")
    can_dash.send("PowerManagementRequest Run", timeout_ms=20000).contains("State changed")
```

---

## Fixtures reference

| Fixture | Scope | Description |
|---|---|---|
| `pic32mk_config` | session | `Pic32mkConfig` — override in your `conftest.py` |
| `pic32mk_initial_pins` | session | Default GPIO state before CPU starts |
| `pic32mk_initial_adc` | session | Default ADC state before CPU starts |
| `_pic32mk_build` | session, autouse | Compiles firmware in workspace before QEMU starts |
| `qemu_proc` | session | Long-running QEMU shared by all session tests |
| `qemu` | class | Isolated QEMU per test class (respects `qemu_state` marker) |
| `gpio` | session | `GPIOHelper` on the session QEMU |
| `qmp_client` | session | Raw `QMPClient` on the session QEMU |

### `qemu_state` marker options

```python
@pytest.mark.qemu_state(
    pins  = [(Pin, bool), ...],    # GPIO levels at boot
    adc   = [(channel, value), ...],  # 12-bit ADC counts at boot
    bms   = {"key": value},        # BMS struct fields injected via GDB at main()
    bms_uart = {"n_devices": 16, "cell_voltage_mv": 3200},  # start BMSMock
)
```

---

## `Pic32mkConfig` reference

```python
from pytest_qemu_pic32mk import Pic32mkConfig

Pic32mkConfig(
    # ── Workspace (required) ─────────────────────────────────────────────────
    project_src_dir = None,       # Path to firmware root.  Plugin creates
                                  # .pytest-qemu-build/, symlinks sources as TARGET/,
                                  # injects bundled wrapper/, runs pymaketool + make.
                                  # Required — must be set.
    project_name    = "PIC32MK-PROJECT",   # Output file prefix (e.g., <name>.boot.bin)
    linker_script   = "firmware/src/config/default/p32MK1024MCM100.ld",
                                  # XC32 .ld path relative to project_src_dir
    build_workspace = None,       # Where workspace is created (default: .pytest-qemu-build/)
    build           = True,       # Set False to reuse pre-built artifacts (CI cache)
    build_env       = {},         # Extra env vars merged into the build subprocess
                                  # e.g. {"RELEASE": "1"} for optimised build
    startup_dir     = None,       # Custom startup dir replacing bundled wrapper/startup/
                                  # Must contain crt0.S, irq_dispatch.S, mk.py
    # ── Artifacts ────────────────────────────────────────────────────────────
    release_dir = "Release",      # where *.boot.bin / *.app.bin / *.rw.bin / *.rw.addr land
                                  # (auto-set to workspace/Release in workspace mode)
    # ── QEMU ─────────────────────────────────────────────────────────────────
    qemu_bin            = "qemu-system-mipsel",  # path or binary on PATH
    gdb_port            = 1234,                  # session QEMU GDB port (VSCode attaches here)
    gdb_port_isolated   = 1235,                  # isolated (class) QEMU GDB port
    qmp_sock            = "/tmp/qemu-qmp.sock",
    qmp_sock_isolated   = "/tmp/qemu-qmp-isolated.sock",
    bms_uart_sock       = "/tmp/qemu-bms-uart.sock",
    bms_uart_sock_isolated = "/tmp/qemu-bms-uart-isolated.sock",
    # List form — sequential from CAN1:
    vcan_interfaces     = ["vcan_pwr_mgmt", "vcan_dashboard"],
    # Dict form — explicit 1-indexed CANFD port numbers (CAN3 → canbus2):
    # vcan_interfaces   = {3: "vcan_pwr_mgmt"},
    # No extra_qemu_args needed for CAN — vcan_interfaces handles both
    # vcan device setup and QEMU -object can-bus / can-host-socketcan args.
    extra_qemu_args     = [],                    # appended verbatim to QEMU command line
)
```

---

## `Makefile.py` — bundled build for QEMU emulation

The plugin bundles a ready-made `Makefile.py` (pymaketool build configuration)
inside the package.  When you use workspace mode, the plugin automatically:

1. Creates `.pytest-qemu-build/` workspace
2. Copies the bundled `Makefile.py` into `pymake/`
3. Symlinks your firmware sources as `TARGET/`
4. Symlinks the bundled MIPS wrapper as `wrapper/`
5. Runs `pymaketool all` then `make -f pymake/makefile.mk all`

**No `Makefile` setup is needed in your firmware project.**

The bundled `Makefile.py` produces three flat binary artifacts that QEMU loads:

| File | QEMU argument | Loaded at | Content |
|---|---|---|---|
| `*.boot.bin` | `-bios` | Boot flash `0xBFC00000` | `.reset` stub only (~152 B) |
| `*.app.bin` | `-global pic32mk-nvm.filename=` | Program flash `0x9D000000` | kseg0 sections (code, ROdata) |
| `*.rw.bin` | `-device loader,file=…,addr=` | RAM | Initialized `.data` segment |

A companion `*.rw.addr` file is written alongside `*.rw.bin` with the physical
load address.

A `compile_commands.json` symlink is created at `.pytest-qemu-build/compile_commands.json`
after every build for clangd IntelliSense support.

### Why three files?

Boot flash (`0xBFC00000`) and program flash (`0x9D000000`) are 480 MB apart in
the MIPS address space.  A single flat binary spanning both regions would be ~1 GB.

### Customizing the build

Workspace mode uses environment variables to parameterize the bundled `Makefile.py`:

```python
Pic32mkConfig(
    project_src_dir=".",
    project_name="MY-FW",
    linker_script="src/custom_linker/linker.ld",  # custom linker script path
    build_workspace=".qemu-build",                 # custom workspace location
    build_env={"RELEASE": "1"},                    # pass RELEASE=1 to pymaketool
)
```

The bundled `Makefile.py` reads:

- `QEMU_PROJECT_NAME` → output file prefix
- `QEMU_LINKER_SCRIPT` → path to XC32 `.ld` (relative to workspace root)
- `RELEASE` → set to "1" for `-O1` optimised build
- `TARGET/` → symlink to your firmware sources
- `wrapper/` → symlink to the bundled MIPS wrapper

To inspect the bundled `Makefile.py`:

```bash
python3 -c "from pytest_qemu_pic32mk import get_wrapper_dir; print(get_wrapper_dir().parent / 'build_assets' / 'Makefile.py')"
```

### Bundled wrapper layout

The plugin includes the MIPS cross-compile glue files needed for QEMU emulation:

```
wrapper/
├── startup/
│   ├── crt0.S              # reset vector @ 0xBFC00000, CP0 init, BSS zero
│   ├── irq_dispatch.S      # interrupt dispatch table for single-vector mode
│   └── mk.py               # pymaketool sub-makefile
├── stubs/
│   ├── libc_stubs.c        # missing C runtime symbols
│   ├── freertos_overrides.c
│   ├── stdio.h / stdlib.h  # minimal freestanding replacements
│   ├── sys/attribs.h       # attribute macros
│   ├── sys/kmem.h          # kernel memory layout
│   └── gnu/stubs-o32_soft.h
└── xc32/
    ├── xc.h                # CP0 register macros + MIPS intrinsics (XC32 compat)
    └── mk.py               # pymaketool sub-makefile
```

These are injected into your workspace automatically via symlink.

---

## Build utilities

### `extract_rw_segment`

Parses the ELF32 RW LOAD segment and writes a flat binary for QEMU RAM
preloading.  Also writes a companion `*.rw.addr` file with the physical load
address.  This is required because our `crt0.S` only zeroes `.bss` — it does
not copy `.data` from flash (no LMA→VMA copy loop, no XC32 `__dinit_copy_val`).
QEMU must preload the initialized globals directly into RAM.

```python
from pytest_qemu_pic32mk import extract_rw_segment

extract_rw_segment(
    elf_path="Release/MY-FIRMWARE.elf",
    out_bin="Release/MY-FIRMWARE.rw.bin",
)
# Produces:
#   Release/MY-FIRMWARE.rw.bin   — raw initialized .data bytes
#   Release/MY-FIRMWARE.rw.addr  — physical load address (e.g. "0x80000010")
```

### `scan_elf` / `validate_objects_dir`

Detects XC32 `__dinit_copy_val_data` fmt=3 bugs — uniform non-zero static
initializers whose size is not divisible by 4.  The XC32 linker emits a
word-loop for such symbols, writing 4 bytes per iteration regardless of object
size, which silently corrupts adjacent memory at startup:

```python
from pytest_qemu_pic32mk import validate_objects_dir

violations = validate_objects_dir("Release/Objects")
for v in violations:
    print(f"{v['file']}::{v['symbol']}  size={v['size']}  {v['reason']}")
```

---

## Full project layout

The simplest setup — no `Makefile`, no pymaketool config in your project:

```
my-firmware-project/
├── pyproject.toml           ← adds pytest-qemu-pic32mk as dev dependency
├── .gitignore               ← add .pytest-qemu-build/
├── .clangd                  ← optional: CompilationDatabase: .pytest-qemu-build
├── firmware/
│   ├── src/
│   │   ├── config/default/p32MK1024MCM100.ld   ← your XC32 linker script
│   │   ├── main.c
│   │   └── … your firmware sources …
│   └── …
└── tests/
    ├── conftest.py          ← minimal: just override pic32mk_config
    ├── signals.py           ← project Pin definitions, ADC channels
    ├── test_digital_inputs.py
    ├── test_voltage.py
    └── test_hvil.py
```

**`tests/conftest.py` (minimal setup):**

```python
import pytest
from pathlib import Path
from pytest_qemu_pic32mk import Pic32mkConfig
from tests.signals import GPI_HVIL, IEP_VOLTAGE, HIGH, V

@pytest.fixture(scope="session")
def pic32mk_config():
    return Pic32mkConfig(
        qemu_bin="/opt/qemu-pic32mk/build/qemu-system-mipsel",
        project_src_dir=Path(__file__).parent.parent / "firmware",
        project_name="MY-FIRMWARE",
        linker_script="src/config/default/p32MK1024MCM100.ld",
        vcan_interfaces=["vcan_pwr_mgmt", "vcan_dashboard"],
    )

@pytest.fixture(scope="session")
def pic32mk_initial_pins():
    return [(GPI_HVIL, HIGH)]

@pytest.fixture(scope="session")
def pic32mk_initial_adc():
    return [(IEP_VOLTAGE, V(690))]
```

**`.gitignore`:**

```
.pytest-qemu-build/
.pytest_cache/
```

**First run:**

```bash
# Run tests (builds firmware automatically)
pytest tests/ -v

# Generate VSCode debug config (one-time setup)
pytest --qemu-vscode-init
```

Expected output from `pytest tests/ -v`:

```
[pytest-qemu-pic32mk] Building firmware (workspace mode)
  workspace : /path/to/my-firmware-project/.pytest-qemu-build
  project   : MY-FIRMWARE
  sources   : /path/to/my-firmware-project/firmware
  linker    : TARGET/src/config/default/p32MK1024MCM100.ld
…pymaketool output…
…make output…
[pytest-qemu-pic32mk] Build OK (workspace).
QEMU: launching...
tests/test_hvil.py::TestHvilOpen::test_fault_is_set PASSED
```
