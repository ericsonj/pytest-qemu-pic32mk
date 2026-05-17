"""Configuration for pytest-qemu-pic32mk.

Create a ``Pic32mkConfig`` and override the ``pic32mk_config`` fixture in your
project's ``conftest.py`` to customise the QEMU binary path, ports, sockets,
and vcan interfaces for your project.

Example ``conftest.py``::

    from pytest_qemu_pic32mk import Pic32mkConfig
    import pytest

    @pytest.fixture(scope="session")
    def pic32mk_config():
        return Pic32mkConfig(
            qemu_bin="/home/user/src/qemu/build/qemu-system-mipsel",
            vcan_interfaces=["vcan_pwr_mgmt", "vcan_dashboard"],
        )
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

VCANInterfacesSpec = list[str | None] | dict[int, str]
"""CAN interface mapping for :attr:`Pic32mkConfig.vcan_interfaces`.

List form (sequential from CAN1)::

    ["vcan_pwr_mgmt", "vcan_dashboard"]   # CAN1→vcan_pwr_mgmt, CAN2→vcan_dashboard

Dict form (explicit 1-indexed firmware CANFD port numbers)::

    {3: "vcan_pwr_mgmt"}   # CAN3→vcan_pwr_mgmt (canbus2 in QEMU)
"""


@dataclass
class Pic32mkConfig:
    """All tuneable settings for the QEMU test harness.

    Attributes:
        qemu_bin:               Path to the ``qemu-system-mipsel`` binary.
                                Must be the custom build with ``pic32mk`` machine
                                support.  Defaults to ``"qemu-system-mipsel"``
                                (looked up on PATH).
        gdb_port:               TCP port for the GDB stub of the *session* QEMU
                                instance.  Default 1234.
        gdb_port_isolated:      TCP port for the GDB stub of the *isolated*
                                (class-scoped) QEMU instance.  Default 1235.
        qmp_sock:               UNIX socket path for QMP on the session instance.
        qmp_sock_isolated:      UNIX socket path for QMP on isolated instances.
        bms_uart_sock:          UNIX socket path for UART1 (BMS) on session instance.
        bms_uart_sock_isolated: UNIX socket path for UART1 on isolated instances.
        vcan_interfaces:        SocketCAN virtual interfaces to create/bring-up
                                and wire to QEMU CAN bus objects.

                                **List form** — sequential from CAN1::

                                    ["vcan_pwr_mgmt", "vcan_dashboard"]
                                    # CAN1→vcan_pwr_mgmt (canbus0)
                                    # CAN2→vcan_dashboard (canbus1)

                                Use ``None`` to skip a port::

                                    [None, None, "vcan_pwr_mgmt"]
                                    # CAN3→vcan_pwr_mgmt (canbus2)

                                **Dict form** — explicit 1-indexed CANFD port numbers
                                (matches firmware ``BOOT_CAN_PORT`` defines)::

                                    {3: "vcan_pwr_mgmt"}
                                    # CAN3→vcan_pwr_mgmt (canbus2)

                                No need to add ``extra_qemu_args`` for CAN wiring;
                                this field handles both kernel interface setup and
                                QEMU ``-object can-bus`` / ``can-host-socketcan`` args.
        extra_qemu_args:        Additional raw arguments appended to the QEMU
                                command line.
        release_dir:            Directory where ``*.boot.bin``, ``*.app.bin``,
                                ``*.rw.bin`` and ``*.rw.addr`` are found after the
                                firmware build.  Relative paths are resolved from
                                the pytest root directory.  Defaults to ``"Release"``.
        build:                  Whether to compile the firmware before running tests.
                                Defaults to ``True``.  Set to ``False`` to skip the
                                build step (use pre-built artifacts in *release_dir*).
        build_cmd:              Shell command used to compile the firmware for QEMU.
                                Defaults to ``"make"``.
                                Override example: ``"poetry run pymaketool"`` or
                                ``"make RELEASE=1"``.
                                Note: if a ``Makefile.py`` is present in
                                ``build_workspace`` but ``pymake/makefile.mk`` does
                                not yet exist, ``pymaketool`` is run automatically
                                before this command.
        build_env:              Extra environment variables merged into the build
                                subprocess environment.  Useful for passing
                                ``RELEASE=1`` or custom toolchain paths without
                                changing the shell environment.
        project_src_dir:        Path to the foreign firmware project root.  When
                                set the plugin uses *workspace mode*: it creates a
                                managed build workspace, symlinks this directory as
                                ``TARGET/``, injects the bundled ``wrapper/``, copies
                                the bundled ``Makefile.py`` into ``pymake/``, and
                                drives ``pymaketool`` + ``make`` transparently —
                                no ``Makefile`` or pymaketool setup is required in
                                the foreign project.
                                Set to ``None`` (default) to use
                                *build_cmd* / *build_workspace* mode.
        project_name:           Output file prefix used as ``QEMU_PROJECT_NAME``
                                when building in workspace mode.  The ``Release/``
                                directory will contain ``<project_name>.boot.bin``,
                                ``<project_name>.app.bin``, etc.
                                Defaults to ``"PIC32MK-PROJECT"``.
        linker_script:          Path to the XC32 linker script *relative to*
                                ``project_src_dir``.  The plugin maps this to
                                ``TARGET/<linker_script>`` inside the workspace.
                                Defaults to the IDU-style path
                                ``"firmware/src/config/default/p32MK1024MCM100.ld"``.
        build_workspace:        Directory where the managed build workspace is
                                created.  Relative paths are resolved from the
                                pytest root directory.  Defaults to
                                ``".pytest-qemu-build"`` next to ``pytest.ini``.
                                Add this directory to ``.gitignore``.
        startup_dir:            Path to a custom startup directory that replaces
                                the bundled ``wrapper/startup/``.  Must contain
                                ``crt0.S``, ``irq_dispatch.S``, and ``mk.py``.
                                Only valid in workspace mode (i.e. when
                                ``project_src_dir`` is set).  Defaults to
                                ``None`` (use the bundled startup).
        stubs_dir:              Path to a custom stubs directory that replaces
                                the bundled ``wrapper/stubs/``.  Must provide
                                the same headers and sources (``libc_stubs.c``,
                                ``freertos_overrides.c``, ``stdlib.h``,
                                ``stdio.h``, ``sys/``, ``gnu/``, and ``mk.py``).
                                Only valid in workspace mode (i.e. when
                                ``project_src_dir`` is set).  Defaults to
                                ``None`` (use the bundled stubs).
        xc32_dir:               Path to a custom xc32 directory that replaces
                                the bundled ``wrapper/xc32/``.  Must provide
                                ``xc.h`` (CP0 macros and MIPS intrinsics) and
                                ``mk.py``.  Only valid in workspace mode (i.e.
                                when ``project_src_dir`` is set).  Defaults to
                                ``None`` (use the bundled xc32 headers).
    """

    qemu_bin: str = "qemu-system-mipsel"
    gdb_port: int = 1234
    gdb_port_isolated: int = 1235
    qmp_sock: str = "/tmp/qemu-qmp.sock"
    qmp_sock_isolated: str = "/tmp/qemu-qmp-isolated.sock"
    bms_uart_sock: str = "/tmp/qemu-bms-uart.sock"
    bms_uart_sock_isolated: str = "/tmp/qemu-bms-uart-isolated.sock"
    vcan_interfaces: VCANInterfacesSpec = field(default_factory=list)
    extra_qemu_args: list[str] = field(default_factory=list)
    release_dir: str | Path = "Release"
    build: bool = True
    build_env: dict[str, str] = field(default_factory=dict)
    project_src_dir: str | Path | None = None
    project_name: str = "PIC32MK-PROJECT"
    linker_script: str = "firmware/src/config/default/p32MK1024MCM100.ld"
    build_workspace: str | Path | None = None
    startup_dir: str | Path | None = None
    stubs_dir: str | Path | None = None
    xc32_dir: str | Path | None = None
