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

### Two build modes

The plugin supports two modes.  **Workspace mode** (new, recommended) is transparent:
the plugin creates an isolated build workspace, symlinks your firmware sources and
the bundled MIPS wrapper, and drives `pymaketool` + `make` automatically — no
`Makefile` or pymaketool configuration needed in your foreign project.

**Custom build mode** (used when `project_src_dir` is unset) runs your `build_cmd`
in `build_workspace` — for projects that already have a `Makefile.py` written
specifically for QEMU emulation.  The firmware always rebuilds; XC32/MPLAB X
embedded-system artifacts cannot be reused.

### 1 — Workspace mode (recommended for foreign projects)

Create a minimal `conftest.py` that points to your firmware sources.  The plugin
handles the rest:

```python
# tests/conftest.py
import pytest
from pathlib import Path
from pytest_qemu_pic32mk import Pic32mkConfig

@pytest.fixture(scope="session")
def pic32mk_config():
    return Pic32mkConfig(
        qemu_bin="/opt/qemu-pic32mk/build/qemu-system-mipsel",
        # ── Workspace mode: set project_src_dir ────────────────────────────
        # Path to the firmware project root (where your .ld, FreeRTOS config live).
        # The plugin creates .pytest-qemu-build/, symlinks your sources as TARGET/,
        # injects the bundled wrapper/, and runs pymaketool + make.
        project_src_dir=Path(__file__).parent.parent,  # one level up
        project_name="MY-FIRMWARE",  # output prefix for *.boot.bin etc.
        # Optional: point to your linker script (relative to project_src_dir).
        # Defaults to "firmware/src/config/default/p32MK1024MCM100.ld"
        linker_script="src/config/default/p32MK1024MCM100.ld",
        # SocketCAN interfaces that QEMU attaches CAN buses to
        vcan_interfaces=["vcan_pwr_mgmt", "vcan_dashboard"],
    )
```

**That's it!**  When you run `pytest`:
1. The plugin creates `.pytest-qemu-build/` workspace
2. Copies bundled `Makefile.py` + helper scripts into `pymake/`
3. Runs `pymaketool all` then `make -f pymake/makefile.mk all` (streaming output)
4. QEMU launches with the fresh `Release/*.boot.bin`, `*.app.bin`, `*.rw.bin`
5. Tests run

Your firmware project needs **zero `Makefile` or pymaketool setup**.  The wrapper files
and build configuration are bundled inside the pytest plugin.

### 1b — Custom build mode (bring your own QEMU-targeted `Makefile.py`)

Use this mode when your project already has its own `Makefile.py` written specifically
for QEMU emulation (using `mipsel-linux-gnu-gcc`, the bundled wrapper, and the three
binary artifact outputs).  The firmware **always rebuilds** — you cannot reuse an
existing embedded-system build because XC32/MPLAB X produces binaries for real hardware
that QEMU cannot load.

```python
@pytest.fixture(scope="session")
def pic32mk_config():
    return Pic32mkConfig(
        qemu_bin="/opt/qemu-pic32mk/build/qemu-system-mipsel",
        build_workspace=".",        # directory containing your Makefile.py / Makefile
        build_cmd="make",           # default; override: "poetry run pymaketool", "make RELEASE=1", …
        release_dir="Release",      # where *.boot.bin / *.app.bin / *.rw.bin land
        vcan_interfaces=["vcan_pwr_mgmt", "vcan_dashboard"],
    )
```

To skip the build and use already-built QEMU artifacts (e.g. in CI with a pre-built cache):

```python
Pic32mkConfig(build=False, release_dir="Release")
```

### 2 — Define your pin descriptors (`signals.py`)


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

### 3 — Set boot-time hardware state

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
| `_pic32mk_build` | session, autouse | Compiles firmware (runs `build_cmd` in `build_dir`) before QEMU starts |
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
    # ── Workspace mode (recommended for foreign projects) ────────────────────
    project_src_dir = None,       # Path to firmware root.  When set, plugin creates
                                  # an isolated build workspace, symlinks this as TARGET/,
                                  # injects bundled wrapper/, and runs pymaketool + make
                                  # transparently.  Set to None (default) for legacy mode.
    project_name    = "PIC32MK-PROJECT",   # Output file prefix (e.g., <name>.boot.bin)
    linker_script   = "firmware/src/config/default/p32MK1024MCM100.ld",  # XC32 .ld path
                                  # (relative to project_src_dir)
    build_workspace = None,       # Where the workspace is created (default: .pytest-qemu-build/)
    # ── Custom build mode (bring your own QEMU-targeted Makefile.py) ─────────
    # Note: firmware ALWAYS rebuilds for QEMU emulation — XC32/MPLAB X embedded
    # system artifacts cannot be reused; they target real hardware, not QEMU.
    build           = True,       # set False only to reuse pre-built QEMU artifacts (CI cache)
    build_workspace = None,       # directory containing Makefile.py/Makefile; defaults to
                                  # project_src_dir or pytest rootdir when unset
    build_cmd       = "make",     # override: "poetry run pymaketool", "make RELEASE=1", …
    build_env       = {},         # extra env vars merged into the build subprocess
    # ── Artifacts ────────────────────────────────────────────────────────
    release_dir = "Release",      # where *.boot.bin / *.app.bin / *.rw.bin / *.rw.addr land
    # ── QEMU ────────────────────────────────────────────────────────────
    qemu_bin            = "qemu-system-mipsel",  # path or binary on PATH
    gdb_port            = 1234,                  # session QEMU GDB port
    gdb_port_isolated   = 1235,                  # isolated (class) QEMU GDB port
    qmp_sock            = "/tmp/qemu-qmp.sock",
    qmp_sock_isolated   = "/tmp/qemu-qmp-isolated.sock",
    bms_uart_sock       = "/tmp/qemu-bms-uart.sock",
    bms_uart_sock_isolated = "/tmp/qemu-bms-uart-isolated.sock",
    vcan_interfaces     = [],                    # e.g. ["vcan_pwr_mgmt", "vcan_dashboard"]
    extra_qemu_args     = [],                    # appended verbatim to QEMU command line
)
```

---

## `Makefile.py` — bundled build for QEMU emulation

The plugin bundles a ready-made `Makefile.py` (pymaketool build configuration)
inside the package.  When you use workspace mode (`project_src_dir` set), the plugin
automatically:

1. Creates `.pytest-qemu-build/` workspace
2. Copies the bundled `Makefile.py` into `pymake/`
3. Symlinks your firmware sources as `TARGET/`
4. Symlinks the bundled MIPS wrapper as `wrapper/`
5. Runs `pymaketool all` then `make -f pymake/makefile.mk all`

**No `Makefile` setup is needed in your foreign project.**

The bundled `Makefile.py` produces three flat binary artifacts that QEMU loads:

| File | QEMU argument | Loaded at | Content |
|---|---|---|---|
| `*.boot.bin` | `-bios` | Boot flash `0xBFC00000` | `.reset` stub only (~152 B) |
| `*.app.bin` | `-global pic32mk-nvm.filename=` | Program flash `0x9D000000` | kseg0 sections (code, ROdata) |
| `*.rw.bin` | `-device loader,file=…,addr=` | RAM | Initialized `.data` segment |

A companion `*.rw.addr` file is written alongside `*.rw.bin` with the physical
load address.

### Why three files?

Boot flash (`0xBFC00000`) and program flash (`0x9D000000`) are 480 MB apart in
the MIPS address space.  A single flat binary spanning both regions would be ~1 GB.

### If you need to customize the build

In rare cases you may want to modify how the firmware compiles (different
source layout, extra includes, custom linker flags, etc.).  Workspace mode
uses environment variables to parameterize the bundled `Makefile.py`:

```python
Pic32mkConfig(
    project_src_dir=".",
    project_name="MY-FW",
    linker_script="src/custom_linker/linker.ld",  # custom linker script path
    build_workspace=".qemu-build",  # custom workspace location
    build_env={"RELEASE": "1"},  # pass RELEASE=1 to pymaketool
)
```

The bundled `Makefile.py` reads:

- `QEMU_PROJECT_NAME` → output file prefix
- `QEMU_LINKER_SCRIPT` → path to XC32 `.ld` (relative to workspace root)
- `RELEASE` → set to "1" for `-O1` optimised build
- `TARGET/` → symlink to your firmware sources
- `wrapper/` → symlink to the bundled MIPS wrapper

If you need even more control, you can create your own `pymake/Makefile.py`
in your project and set `legacy mode` instead (`build_dir="."`, `build_cmd="poetry run pymaketool"`).
Copy the bundled example from the plugin source or from IDU-EMULATOR:

```bash
# Bundled version inside the installed package
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

These are injected into your workspace automatically; no need to copy them manually.

### Complete custom `Makefile.py` example (if you don't use workspace mode)

```python
# Makefile.py  (pymaketool build configuration)
import os
import re
import subprocess

from pymakelib import MKVARS, AbstractMake, Makeclass, addon
from pymakelib.clangd_addon import CompileCommandsAddon
from pytest_qemu_pic32mk import get_wrapper_dir, extract_rw_segment, validate_objects_dir

# ── clangd compile_commands.json support ──────────────────────────────────────
CompileCommandsAddon.strip_flags = ["-fframe-base-loclist", "-MP", "-MMD", "-c"]
CompileCommandsAddon.strip_flags_with_value = ["-mprocessor", "-mdfp", "-mreserve"]
addon.add(CompileCommandsAddon)

# ── Project settings ──────────────────────────────────────────────────────────
PROJECT_NAME   = "MY-FIRMWARE"
RELEASE_BUILD  = os.environ.get("RELEASE", "0") == "1"
FOLDER_OUT     = "Release/Objects/"
DIST_DIR       = "Release/"

# Bundled wrapper files from pytest-qemu-pic32mk
WRAPPER = get_wrapper_dir()

# Resolve GCC built-in include (for stdint.h / stddef.h with -nostdinc)
GCC_INTERNAL_INCLUDE = subprocess.check_output(
    ["mipsel-linux-gnu-gcc", "-print-file-name=include"], text=True
).strip()

# Resolve libgcc.a for the target ABI
_LIBGCC_BASE = os.path.dirname(
    subprocess.check_output(
        ["mipsel-linux-gnu-gcc", "-print-libgcc-file-name"], text=True
    ).strip()
)
_LIBGCC_MULTI = subprocess.check_output(
    ["mipsel-linux-gnu-gcc",
     "-march=mips32r2", "-mabi=32", "-mhard-float", "-mfp64", "-EL",
     "-print-multi-directory"],
    text=True,
).strip()
LIBGCC = (
    os.path.join(_LIBGCC_BASE, "libgcc.a")
    if _LIBGCC_MULTI == "."
    else os.path.join(_LIBGCC_BASE, _LIBGCC_MULTI, "libgcc.a")
)

# ── Output artifacts ──────────────────────────────────────────────────────────
TARGET_ELF       = DIST_DIR + PROJECT_NAME + ".elf"
TARGET_MAP       = DIST_DIR + PROJECT_NAME + ".map"
TARGET_BOOT_BIN  = DIST_DIR + PROJECT_NAME + ".boot.bin"   # .reset only
TARGET_PFLASH    = DIST_DIR + PROJECT_NAME + ".app.bin"    # kseg0 sections
TARGET_RW_BIN    = DIST_DIR + PROJECT_NAME + ".rw.bin"     # initialized .data

# ── Linker script preprocessing ───────────────────────────────────────────────
#
# XC32's linker script uses OUTPUT_ARCH(pic32mx) and C preprocessor directives
# (#if / #endif) that mipsel-linux-gnu-ld cannot handle.  Fix both:
#   1. Run through gcc -E -P to evaluate #if blocks.
#   2. Replace OUTPUT_ARCH(pic32mx) → OUTPUT_ARCH(mips).
#   3. Remove OPTIONAL(...) calls (GNU ld has no OPTIONAL() built-in).
#   4. Inject .simple_tlb_refill at 0x9D000000 so objcopy starts app.bin
#      at the correct base address.
#   5. Inject .vector_dispatch at EBASE+0x200 for the interrupt dispatcher.
_LD_SRC = "src/config/default/p32MK1024MCM100.ld"   # ← adjust to your project
LINKER_SCRIPT = DIST_DIR + "linker_script.ld"

os.makedirs(DIST_DIR, exist_ok=True)
_ld = subprocess.check_output(
    ["mipsel-linux-gnu-gcc", "-E", "-P", "-x", "c", _LD_SRC], text=True
)
_ld = _ld.replace("OUTPUT_ARCH(pic32mx)", "OUTPUT_ARCH(mips)")
_ld = re.sub(r"OPTIONAL\s*\([^)]*\)\s*;?\n?", "", _ld)

# Inject .simple_tlb_refill so program-flash binary starts at 0x9D000000
_TLB = (
    "\n  .simple_tlb_refill _SIMPLE_TLB_REFILL_EXCPT_ADDR :\n"
    "  {\n    KEEP(*(.simple_tlb_refill_excpt))\n  } > kseg0_exception_mem\n"
)
_ld = _ld.replace(".app_excpt _GEN_EXCPT_ADDR :", _TLB + "  .app_excpt _GEN_EXCPT_ADDR :")

# Inject .vector_dispatch at EBASE+0x200 (single-vector mode, IntCtl.VS=0)
_ld = _ld.replace(
    "    __vector_offset_0 = (DEFINED(__vector_dispatch_0)"
    " ? (. - _ebase_address) : __vector_offset_default);",
    "    KEEP(*(.vector_dispatch))\n    . = ALIGN(4) ;\n"
    "    __vector_offset_0 = (DEFINED(__vector_dispatch_0)"
    " ? (. - _ebase_address) : __vector_offset_default);",
)
with open(LINKER_SCRIPT, "w") as _f:
    _f.write(_ld)


@Makeclass
class Project(AbstractMake):

    def getProjectSettings(self, **kwargs) -> dict:
        return {"PROJECT_NAME": PROJECT_NAME, "FOLDER_OUT": FOLDER_OUT}

    def getCompilerSet(self, **kwargs) -> dict:
        prefix = "mipsel-linux-gnu-"
        return {
            "CC":      prefix + "gcc",
            "CXX":     prefix + "g++",
            "LD":      prefix + "ld",
            "AR":      prefix + "ar",
            "AS":      prefix + "as",
            "OBJCOPY": prefix + "objcopy",
            "SIZE":    prefix + "size",
            "INCLUDES": [
                GCC_INTERNAL_INCLUDE,
                "/usr/mipsel-linux-gnu/include",
                str(WRAPPER / "stubs"),   # stdio.h, stdlib.h, sys/, gnu/
                str(WRAPPER / "xc32"),    # xc.h  (CP0 macros, MIPS intrinsics)
            ],
        }

    def getCompilerOpts(self, **kwargs) -> dict:
        return {
            "MACROS": {"MCU_PIC32MK1024GPE100": 1, "__mips_hard_float": 1}
                      | ({} if RELEASE_BUILD else {"DEBUG": "1"}),
            "MACHINE-OPTS": [
                "-march=mips32r2", "-mdspr2", "-mhard-float", "-mfp64",
                "-mno-micromips", "-mabi=32", "-EL", "-G", "0", "-mno-abicalls",
            ],
            "OPTIMIZE-OPTS":   ["-O1"] if RELEASE_BUILD else ["-Og"],
            "OPTIONS": [
                "-ffreestanding", "-fno-builtin", "-nostdlib", "-nostdinc",
                "-ffunction-sections", "-fdata-sections",
            ] + ([] if RELEASE_BUILD else ["-fno-omit-frame-pointer"]),
            "DEBUGGING-OPTS":  ["-g"] if RELEASE_BUILD else ["-g3", "-gdwarf-2"],
            "WARNINGS-OPTS":   ["-Wno-unused-parameter", "-Wno-sign-compare"],
            "CONTROL-C-OPTS":  ["-c"],
            "GENERAL-OPTS": [
                "-isystem", GCC_INTERNAL_INCLUDE,
                "-isystem", "/usr/mipsel-linux-gnu/include",
                "-include", str(WRAPPER / "stubs" / "stdlib.h"),
            ],
            "PREPROCESSOR-OPTS": [],
        }

    def getLinkerOpts(self, **kwargs) -> dict:
        return {
            "LINKER-SCRIPT": [],
            "MACHINE-OPTS":  [],
            "GENERAL-OPTS":  [],
            "LINKER-OPTS":   [],
        }

    def getTargetsScript(self, **kwargs) -> dict:
        return {
            # ── Link ──────────────────────────────────────────────────────────
            "TARGET": {
                "LOGKEY": "LD",
                "FILE": TARGET_ELF,
                "SCRIPT": [
                    MKVARS.LD,
                    "-m", "elf32ltsmip",
                    "-T", LINKER_SCRIPT,
                    "--gc-sections",
                    "--wrap=vApplicationIdleHook",
                    "-Map=" + TARGET_MAP,
                    "-o", "$@",
                    MKVARS.OBJECTS,
                    LIBGCC,
                ],
            },
            # ── Boot flash image (.reset stub at 0xBFC00000, ~152 B) ──────────
            "TARGET_BOOT_BIN": {
                "LOGKEY": "BOOT",
                "FILE": TARGET_BOOT_BIN,
                "SCRIPT": [
                    MKVARS.OBJCOPY,
                    "-O", "binary",
                    "-j", ".reset",
                    MKVARS.TARGET, TARGET_BOOT_BIN,
                ],
            },
            # ── Program flash image (kseg0 sections, LMA 0x9Dxxxxxx) ──────────
            # --wildcard lets -j accept glob patterns.  Only include sections
            # with LMAs in program flash; .data* has LMAs in RAM (0x80000010)
            # and would cause objcopy to span a ~486 MB gap.
            "TARGET_PFLASH_BIN": {
                "LOGKEY": "PFLASH",
                "FILE": TARGET_PFLASH,
                "SCRIPT": [
                    MKVARS.OBJCOPY,
                    "-O", "binary", "--wildcard",
                    "-j", ".simple_tlb_refill*",
                    "-j", ".app_excpt",
                    "-j", ".vectors",
                    "-j", ".text*",
                    "-j", ".rodata*",
                    "-j", ".dinit*",
                    MKVARS.TARGET, TARGET_PFLASH,
                ],
            },
            # ── RAM init image (.data segment for QEMU RAM preload) ───────────
            # extract_rw_segment parses the ELF32 LOAD RW segment and writes
            # TARGET_RW_BIN + TARGET_RW_BIN.rw.addr (physical load address).
            "TARGET_RAM_BIN": {
                "LOGKEY": "RWINIT",
                "FILE": TARGET_RW_BIN,
                "SCRIPT": [
                    "python3", "-c",
                    f"from pytest_qemu_pic32mk import extract_rw_segment; "
                    f"extract_rw_segment('{TARGET_ELF}', '{TARGET_RW_BIN}')",
                ],
            },
            # ── XC32 fmt=3 static-init bug check ─────────────────────────────
            "TARGET_CHECKS": {
                "LOGKEY": "CHK",
                "FILE": "CHECKS",
                "SCRIPT": [
                    "python3", "-c",
                    f"from pytest_qemu_pic32mk import validate_objects_dir; "
                    f"v = validate_objects_dir('{FOLDER_OUT}'); "
                    f"[print(x) for x in v]; exit(1 if v else 0)",
                ],
            },
        }

    def getSources(self, **kwargs) -> list:
        return [
            # ── Wrapper (QEMU emulation glue) ─────────────────────────────────
            str(WRAPPER / "startup" / "crt0.S"),          # reset vector, CP0 init, BSS zero
            str(WRAPPER / "startup" / "irq_dispatch.S"),  # IRQ dispatch table
            str(WRAPPER / "stubs"   / "libc_stubs.c"),    # missing C runtime symbols
            str(WRAPPER / "stubs"   / "freertos_overrides.c"),

            # ── Your firmware sources ─────────────────────────────────────────
            "src/main.c",
            # … add your application sources here …
        ]
```

### Bundled wrapper layout

```
wrapper/
├── startup/
│   ├── crt0.S              # reset vector @ 0xBFC00000, CP0 init, BSS zero, USB ISR trampoline
│   └── irq_dispatch.S      # MIPS exception + IRQ dispatch table
├── stubs/
│   ├── libc_stubs.c        # missing C runtime symbols (__assert_func, etc.)
│   ├── freertos_overrides.c
│   ├── stdio.h / stdlib.h  # minimal freestanding replacements
│   ├── sys/attribs.h
│   ├── sys/kmem.h
│   └── gnu/stubs-o32_soft.h
└── xc32/
    └── xc.h                # CP0 register macros + MIPS intrinsics (XC32 <xc.h> compat)
```

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

## Full project layout — workspace mode example

The simplest setup: no `Makefile`, no pymaketool config in your project.

```
my-firmware-project/
├── pyproject.toml           ← adds pytest-qemu-pic32mk as dev dependency
├── src/
│   ├── config/default/p32MK1024MCM100.ld   ← your XC32 linker script
│   ├── main.c
│   └── … your firmware sources …
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
        project_src_dir=Path(__file__).parent.parent,  # firmware root
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

Run tests — the build happens automatically in `.pytest-qemu-build/`:

```bash
pytest tests/ -v
# [pytest-qemu-pic32mk] Building firmware (workspace mode)
#   workspace : /path/to/my-firmware-project/.pytest-qemu-build
#   project   : MY-FIRMWARE
#   sources   : /path/to/my-firmware-project
#   linker    : TARGET/src/config/default/p32MK1024MCM100.ld
# …pymaketool output…
# …make output…
# [pytest-qemu-pic32mk] Build OK (workspace).
# QEMU: launching...
# tests/test_hvil.py::TestHvilOpen::test_fault_is_set PASSED
```

---

## Full project layout — custom build mode example

Use this when your project has its own `Makefile.py` written for QEMU emulation
(using `mipsel-linux-gnu-gcc` + the bundled wrapper).  The build **always runs** —
you cannot reuse embedded-system artifacts from XC32/MPLAB X.

```
my-firmware-project/
├── pyproject.toml
├── Makefile.py              ← your QEMU-targeted pymaketool config (imports get_wrapper_dir)
├── Makefile                 ← delegates to pymaketool (or just use build_cmd="poetry run pymaketool")
├── Release/                 ← QEMU build artifacts (*.boot.bin, *.app.bin, *.rw.bin)
└── tests/
    ├── conftest.py
    ├── signals.py
    └── test_*.py
```

**`tests/conftest.py` (custom build mode):**

```python
import pytest
from pytest_qemu_pic32mk import Pic32mkConfig
from tests.signals import GPI_HVIL, IEP_VOLTAGE, HIGH, V

@pytest.fixture(scope="session")
def pic32mk_config():
    return Pic32mkConfig(
        qemu_bin="/opt/qemu-pic32mk/build/qemu-system-mipsel",
        # No project_src_dir → custom build mode: runs build_cmd in build_workspace
        build_workspace=".",           # directory containing Makefile.py / Makefile
        build_cmd="make",              # or "poetry run pymaketool"
        release_dir="Release",
        vcan_interfaces=["vcan_pwr_mgmt", "vcan_dashboard"],
    )

@pytest.fixture(scope="session")
def pic32mk_initial_pins():
    return [(GPI_HVIL, HIGH)]

@pytest.fixture(scope="session")
def pic32mk_initial_adc():
    return [(IEP_VOLTAGE, V(690))]
```

Run tests:

```bash
pytest tests/ -v
# [pytest-qemu-pic32mk] Building firmware for QEMU: 'make'  (cwd=/path/to/project)
# …pymaketool / compiler output…
# [pytest-qemu-pic32mk] Build OK.
# QEMU: launching...
# tests/test_hvil.py::TestHvilOpen::test_fault_is_set PASSED
```
