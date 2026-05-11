"""Build utilities for PIC32MK QEMU firmware.

Two functions are bundled from the IDU-EMULATOR project:

* :func:`extract_rw_segment` — extract the initialized RW LOAD segment from
  an ELF32 for QEMU RAM init.  Produces ``*.rw.bin`` and ``*.rw.addr``.

* :func:`scan_elf` — detect initialized static variables that would trigger
  the XC32 ``__dinit_copy_val_data`` size bug (fmt=3, size % 4 != 0).

These are called automatically by the pymaketool build when the
``TARGET_RAM_BIN`` and ``TARGET_CHECKS`` targets are built, but you can also
call them directly from Python.

Example — run validation on all object files after a build::

    from pytest_qemu_pic32mk.build_utils import validate_objects_dir
    violations = validate_objects_dir("Release/Objects")
    if violations:
        for v in violations:
            print(v)

Access the bundled wrapper directory for use in a foreign project's
``Makefile.py``::

    from pytest_qemu_pic32mk import get_wrapper_dir

    WRAPPER = str(get_wrapper_dir())
    # Then reference wrapper/startup/crt0.S, wrapper/stubs/, wrapper/xc32/xc.h
    # in your pymaketool build configuration.
"""

from __future__ import annotations

import os
import struct
import sys
from pathlib import Path
from typing import TYPE_CHECKING

# ── ELF32 constants ───────────────────────────────────────────────────────────

PT_LOAD = 1
PF_W = 2

SHT_SYMTAB   = 2
SHT_STRTAB   = 3
SHT_PROGBITS = 1
SHF_ALLOC    = 0x2
SHF_WRITE    = 0x1
STT_OBJECT   = 1
MIN_SYM_SIZE = 4

# MIPS virtual-to-physical
KSEG0_BASE = 0x80000000
KSEG0_END  = 0xA0000000
KSEG1_BASE = 0xA0000000
KSEG1_END  = 0xC0000000


def _virt_to_phys(vaddr: int) -> int:
    if KSEG0_BASE <= vaddr < KSEG0_END:
        return vaddr - KSEG0_BASE
    if KSEG1_BASE <= vaddr < KSEG1_END:
        return vaddr - KSEG1_BASE
    return vaddr


# ── RW segment extraction ─────────────────────────────────────────────────────

def extract_rw_segment(elf_path: str, out_bin: str) -> None:
    """Extract the initialized RW LOAD segment from *elf_path* into *out_bin*.

    Also writes ``<out_bin_stem>.rw.addr`` with the physical load address in
    hex (e.g. ``0x80000010``).  If no RW segment exists both files are created
    empty / zero-addressed so Make doesn't re-run the target.
    """
    out_addr = os.path.splitext(out_bin)[0] + ".addr"

    with open(elf_path, "rb") as f:
        e_ident = f.read(16)
        if e_ident[:4] != b"\x7fELF":
            raise ValueError(f"{elf_path} is not an ELF file")
        if e_ident[4] != 1:
            raise ValueError("only ELF32 is supported")

        endian = "<" if e_ident[5] == 1 else ">"
        hdr = f.read(36)
        e_phoff    = struct.unpack_from(f"{endian}I", hdr, 12)[0]
        e_phentsize = struct.unpack_from(f"{endian}H", hdr, 26)[0]
        e_phnum    = struct.unpack_from(f"{endian}H", hdr, 28)[0]

        for i in range(e_phnum):
            f.seek(e_phoff + i * e_phentsize)
            phdr = f.read(32)
            p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, _ = \
                struct.unpack_from(f"{endian}8I", phdr)

            if p_type != PT_LOAD or not (p_flags & PF_W) or p_filesz == 0:
                continue

            f.seek(p_offset)
            data = f.read(p_filesz)

            with open(out_bin, "wb") as out:
                out.write(data)

            phys = _virt_to_phys(p_vaddr)
            with open(out_addr, "w") as out:
                out.write(f"0x{phys:08x}\n")

            print(f"  RWINIT\t{out_bin}  "
                  f"({p_filesz} bytes, VMA=0x{p_vaddr:08x} -> phys=0x{phys:08x})")
            return

    # No RW segment
    open(out_bin, "wb").close()
    with open(out_addr, "w") as out:
        out.write("0x00000000\n")
    print("  RWINIT\t(no initialized data segment found)")


# ── XC32 fmt=3 validation ─────────────────────────────────────────────────────

def _is_fmt3_trigger(data: bytes) -> bool:
    if len(data) < 4:
        return False
    if all(b == 0 for b in data):
        return False
    fill = data[0:4]
    for i in range(0, len(data), 4):
        chunk = data[i:i + 4]
        if chunk != fill[:len(chunk)]:
            return False
    return True


def scan_elf(path: str) -> list[dict]:
    """Scan one ELF32 file for XC32 fmt=3 ``__dinit_copy_val_data`` risks.

    Returns a list of violation dicts with keys:
    ``file``, ``symbol``, ``section``, ``size``, ``reason``.
    """
    violations: list[dict] = []
    try:
        with open(path, "rb") as f:
            e_ident = f.read(16)
            if e_ident[:4] != b"\x7fELF":
                return []
            if e_ident[4] != 1:
                return []

            endian = "<" if e_ident[5] == 1 else ">"
            hdr = f.read(36)
            e_shoff     = struct.unpack_from(f"{endian}I", hdr, 16)[0]
            e_shentsize = struct.unpack_from(f"{endian}H", hdr, 30)[0]
            e_shnum     = struct.unpack_from(f"{endian}H", hdr, 32)[0]
            e_shstrndx  = struct.unpack_from(f"{endian}H", hdr, 34)[0]

            def read_shdr(idx):
                f.seek(e_shoff + idx * e_shentsize)
                raw = f.read(e_shentsize)
                if len(raw) < 40:
                    return None
                sn, st, sf, sa, so, ss, sl, _i, _a, se = struct.unpack_from(f"{endian}10I", raw)
                return {"name_idx": sn, "type": st, "flags": sf, "addr": sa,
                        "offset": so, "size": ss, "link": sl, "entsize": se}

            shstrtab = read_shdr(e_shstrndx)
            if shstrtab is None:
                return []

            f.seek(shstrtab["offset"])
            shstrtab_data = f.read(shstrtab["size"])

            def section_name(name_idx):
                end = shstrtab_data.index(b"\x00", name_idx)
                return shstrtab_data[name_idx:end].decode("ascii", errors="replace")

            # find symtab + strtab
            symtab = strtab_data = None
            for i in range(e_shnum):
                s = read_shdr(i)
                if s is None:
                    continue
                if s["type"] == SHT_SYMTAB:
                    symtab = s
                    stab = read_shdr(s["link"])
                    if stab:
                        f.seek(stab["offset"])
                        strtab_data = f.read(stab["size"])

            if symtab is None or strtab_data is None:
                return []

            n_syms = symtab["size"] // symtab["entsize"]
            for i in range(n_syms):
                f.seek(symtab["offset"] + i * symtab["entsize"])
                raw = f.read(symtab["entsize"])
                if len(raw) < 16:
                    continue
                st_name, st_value, st_size, st_info, _other, st_shndx = \
                    struct.unpack_from(f"{endian}IIIBBH", raw)
                sym_type = st_info & 0xF
                if sym_type != STT_OBJECT or st_size < MIN_SYM_SIZE:
                    continue
                if st_shndx == 0 or st_shndx >= e_shnum:
                    continue
                sec = read_shdr(st_shndx)
                if sec is None:
                    continue
                if not (sec["flags"] & SHF_ALLOC and sec["flags"] & SHF_WRITE):
                    continue

                sym_off_in_sec = st_value - sec["addr"] if sec["addr"] else 0
                file_off = sec["offset"] + sym_off_in_sec
                if file_off + st_size > os.path.getsize(path):
                    continue

                f.seek(file_off)
                data = f.read(st_size)
                if not _is_fmt3_trigger(data):
                    continue

                # Risk: uniform non-zero fill with size % 4 != 0
                if st_size % 4 == 0:
                    continue

                end = strtab_data.index(b"\x00", st_name)
                sym_name = strtab_data[st_name:end].decode("ascii", errors="replace")
                violations.append({
                    "file":    path,
                    "symbol":  sym_name,
                    "section": section_name(sec["name_idx"]),
                    "size":    st_size,
                    "reason":  f"size={st_size} not divisible by 4; uniform non-zero init → XC32 fmt=3 loop",
                })
    except (OSError, struct.error):
        pass
    return violations


def validate_objects_dir(objects_dir: str) -> list[dict]:
    """Scan all ``*.o`` files under *objects_dir* for XC32 fmt=3 risks.

    Returns a flat list of violation dicts (see :func:`scan_elf`).
    """
    all_violations: list[dict] = []
    for obj in Path(objects_dir).rglob("*.o"):
        all_violations.extend(scan_elf(str(obj)))
    return all_violations
