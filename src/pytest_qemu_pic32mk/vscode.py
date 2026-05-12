"""VSCode debug configuration generator for pytest-qemu-pic32mk.

Generates and merges launch.json + tasks.json entries into the consumer
firmware project's .vscode/ directory so the developer can debug firmware
running in QEMU with one click (F5).

Workflow
--------
1. Run  ``pytest --qemu-start-debug``  in a terminal.
   Firmware builds, QEMU starts halted at the reset vector, and the GDB
   stub listens on ``localhost:<gdb_port>`` (default 1234).

2. In VSCode: Run & Debug → "QEMU Debug — <project_name>" → F5.
   VSCode runs the background task first (which is the same command as
   step 1 when triggered from VSCode), then attaches gdb-multiarch.
"""

from __future__ import annotations

import json
from pathlib import Path

from .config import Pic32mkConfig
from .gdb_snapshot import _GDB_EXPAND_SCRIPT

_LAUNCH_VERSION = "0.2.0"
_TASKS_VERSION = "2.0.0"

_QEMU_TASK_LABEL = "QEMU: Start for debugging"
_QEMU_KILL_TASK_LABEL = "Kill poetry pytest QEMU Debug"


def _resolve_workspace(config: Pic32mkConfig, rootdir: Path) -> Path:
    """Return the absolute workspace path (mirrors _setup_workspace logic)."""
    if config.build_workspace is not None:
        ws = Path(str(config.build_workspace))
        if not ws.is_absolute():
            ws = rootdir / ws
    else:
        ws = rootdir / ".pytest-qemu-build"
    return ws.resolve()


def _read_json_with_comments(path: Path) -> dict:
    """Read a JSON file, stripping // line comments (VSCode allows them)."""
    lines = path.read_text().splitlines()
    stripped = []
    for line in lines:
        # Remove inline // comments outside string literals (best-effort).
        # Only strip lines that start with optional whitespace + //.
        stripped_line = line.rstrip()
        if stripped_line.lstrip().startswith("//"):
            continue
        stripped.append(stripped_line)
    return json.loads("\n".join(stripped))


def _write_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n")


def _make_launch_entry(
    config: Pic32mkConfig,
    rootdir: Path,
    workspace: Path,
) -> dict:
    """Build the cppdbg launch configuration dict."""
    elf_rel = workspace.relative_to(rootdir) / "Release" / f"{config.project_name}.elf"
    src_dir = Path(str(config.project_src_dir)).resolve()

    return {
        "name": f"QEMU Debug — {config.project_name}",
        "type": "cppdbg",
        "request": "launch",
        "program": "${workspaceFolder}/" + elf_rel.as_posix(),
        "cwd": "${workspaceFolder}",
        "MIMode": "gdb",
        "miDebuggerPath": "gdb-multiarch",
        "miDebuggerServerAddress": f"localhost:{config.gdb_port}",
        "stopAtConnect": True,
        "preLaunchTask": _QEMU_TASK_LABEL,
        "postDebugTask": _QEMU_KILL_TASK_LABEL,
        "setupCommands": [
            {"text": "set architecture mips:isa32r2", "ignoreFailures": False},
            {"text": "set endian little", "ignoreFailures": False},
            {"text": "set remotetimeout 30", "ignoreFailures": False},
            {
                "description": "Step into functions even when source not resolved yet",
                "text": "set step-mode on",
                "ignoreFailures": False,
            },
            {"text": f"source {_GDB_EXPAND_SCRIPT}", "ignoreFailures": False},
            {
                "description": "Enable pretty printing",
                "text": "-enable-pretty-printing",
                "ignoreFailures": True,
            },
        ],
        "sourceFileMap": {
            str(workspace / "TARGET"): str(src_dir),
        },
    }


def _make_task_entry() -> dict:
    """Build the background VSCode task that starts QEMU for debugging."""
    return {
        "label": _QEMU_TASK_LABEL,
        "type": "shell",
        "command": "poetry run pytest --qemu-start-debug -v -s",
        "isBackground": True,
        "problemMatcher": {
            "pattern": {"regexp": "^NEVER$"},
            "background": {
                "activeOnStart": True,
                "beginsPattern": r"\[pytest-qemu-pic32mk\] Building firmware",
                "endsPattern": r"GDB stub\s*:",
            },
        },
        "presentation": {
            "reveal": "always",
            "panel": "dedicated",
            "close": False,
        },
    }


def _make_kill_task_entry() -> dict:
    """Build the VSCode task that kills the QEMU debug process after debugging."""
    return {
        "label": _QEMU_KILL_TASK_LABEL,
        "type": "shell",
        "command": 'pkill -f "poetry run pytest --qemu-start-debug" || true',
        "problemMatcher": [],
    }


def _merge_launch(vscode_dir: Path, entry: dict) -> None:
    """Create or update .vscode/launch.json — adds/replaces the QEMU entry."""
    launch_path = vscode_dir / "launch.json"

    if launch_path.exists():
        try:
            data = _read_json_with_comments(launch_path)
        except (json.JSONDecodeError, OSError):
            data = {"version": _LAUNCH_VERSION, "configurations": []}
    else:
        data = {"version": _LAUNCH_VERSION, "configurations": []}

    confs = data.setdefault("configurations", [])
    # Replace existing entry with same name, or append.
    idx = next((i for i, c in enumerate(confs) if c.get("name") == entry["name"]), None)
    if idx is not None:
        confs[idx] = entry
    else:
        confs.append(entry)

    _write_json(launch_path, data)


def _merge_tasks(vscode_dir: Path, entry: dict) -> None:
    """Create or update .vscode/tasks.json — adds/replaces the QEMU task."""
    tasks_path = vscode_dir / "tasks.json"

    if tasks_path.exists():
        try:
            data = _read_json_with_comments(tasks_path)
        except (json.JSONDecodeError, OSError):
            data = {"version": _TASKS_VERSION, "tasks": []}
    else:
        data = {"version": _TASKS_VERSION, "tasks": []}

    tasks = data.setdefault("tasks", [])
    idx = next(
        (i for i, t in enumerate(tasks) if t.get("label") == entry["label"]), None
    )
    if idx is not None:
        tasks[idx] = entry
    else:
        tasks.append(entry)

    _write_json(tasks_path, data)


def generate_vscode_debug_config(config: Pic32mkConfig, rootdir: Path) -> None:
    """Generate/update .vscode/launch.json and tasks.json for QEMU GDB debugging.

    Merges a ``"QEMU Debug — <project_name>"`` launch configuration and a
    ``"QEMU: Start for debugging"`` background task into the consumer project's
    ``.vscode/`` directory, preserving all existing entries.

    Args:
        config:  Active :class:`Pic32mkConfig` (must have ``project_src_dir`` set).
        rootdir: Consumer project root (pytest ``config.rootdir``).
    """
    if config.project_src_dir is None:
        raise ValueError(
            "generate_vscode_debug_config requires workspace mode "
            "(Pic32mkConfig.project_src_dir must be set)."
        )

    workspace = _resolve_workspace(config, rootdir)
    vscode_dir = rootdir / ".vscode"
    vscode_dir.mkdir(exist_ok=True)

    launch_entry = _make_launch_entry(config, rootdir, workspace)
    task_entry = _make_task_entry()
    kill_task_entry = _make_kill_task_entry()

    _merge_launch(vscode_dir, launch_entry)
    _merge_tasks(vscode_dir, task_entry)
    _merge_tasks(vscode_dir, kill_task_entry)

    print(
        f"[pytest-qemu-pic32mk] VSCode debug config written:\n"
        f"  {vscode_dir / 'launch.json'}\n"
        f"  {vscode_dir / 'tasks.json'}\n"
        f"\n"
        f"  Usage:\n"
        f"    Run & Debug → 'QEMU Debug — {config.project_name}' → F5\n"
        f"    (Starts QEMU automatically, then attaches gdb-multiarch)\n",
        flush=True,
    )
