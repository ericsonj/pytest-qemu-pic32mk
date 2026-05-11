#!/usr/bin/env python3
"""Extract the initialized RW LOAD segment from an ELF32 for QEMU RAM init.

When running PIC32MK firmware via QEMU's -bios/-pflash flat binaries, the
.data section initializers are not automatically copied to RAM (our minimal
crt0 only zeroes .bss).  This script extracts the LOAD RW segment from the
linked ELF and produces two files consumed by start-qemu.sh:

    <output>.rw.bin   – raw bytes of the initialized data segment
    <output>.rw.addr  – physical address (hex) where QEMU should load it

Usage (called automatically by the build system via pymaketool):
    python3 gen_rw_image.py <elf_file> <output_rw_bin>
"""

import os
import struct
import sys

# ELF constants
PT_LOAD = 1
PF_W = 2

# MIPS virtual-to-physical mappings (fixed by architecture)
KSEG0_BASE = 0x80000000
KSEG0_END = 0xA0000000
KSEG1_BASE = 0xA0000000
KSEG1_END = 0xC0000000


def virt_to_phys(vaddr: int) -> int:
    """Convert a MIPS kseg0/kseg1 virtual address to physical."""
    if KSEG0_BASE <= vaddr < KSEG0_END:
        return vaddr - KSEG0_BASE
    if KSEG1_BASE <= vaddr < KSEG1_END:
        return vaddr - KSEG1_BASE
    return vaddr


def extract_rw_segment(elf_path: str, out_bin: str) -> None:
    out_addr = os.path.splitext(out_bin)[0] + ".addr"

    with open(elf_path, "rb") as f:
        # --- ELF32 header ---
        e_ident = f.read(16)
        if e_ident[:4] != b"\x7fELF":
            sys.exit(f"error: {elf_path} is not an ELF file")
        if e_ident[4] != 1:  # EI_CLASS: 1 = 32-bit
            sys.exit("error: only ELF32 is supported")

        endian = "<" if e_ident[5] == 1 else ">"  # EI_DATA: 1 = LE

        hdr = f.read(36)  # remaining 36 bytes of ELF32 header (52 - 16)
        e_phoff = struct.unpack_from(f"{endian}I", hdr, 12)[0]
        e_phentsize = struct.unpack_from(f"{endian}H", hdr, 26)[0]
        e_phnum = struct.unpack_from(f"{endian}H", hdr, 28)[0]

        # --- Scan program headers for the first LOAD RW segment ---
        for i in range(e_phnum):
            f.seek(e_phoff + i * e_phentsize)
            phdr = f.read(32)  # ELF32 Phdr is 32 bytes
            p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_flags, _ = \
                struct.unpack_from(f"{endian}8I", phdr)

            if p_type != PT_LOAD or not (p_flags & PF_W) or p_filesz == 0:
                continue

            # Extract segment bytes
            f.seek(p_offset)
            data = f.read(p_filesz)

            with open(out_bin, "wb") as out:
                out.write(data)

            phys = virt_to_phys(p_vaddr)
            with open(out_addr, "w") as out:
                out.write(f"0x{phys:08x}\n")

            print(f"  RWINIT\t{out_bin}  "
                  f"({p_filesz} bytes, VMA=0x{p_vaddr:08x} -> phys=0x{phys:08x})")
            return

    # No RW segment found — create empty marker so Make doesn't re-run
    open(out_bin, "wb").close()
    with open(out_addr, "w") as out:
        out.write("0x00000000\n")
    print(f"  RWINIT\t(no initialized data segment found)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <elf_file> <output_rw_bin>", file=sys.stderr)
        sys.exit(1)
    extract_rw_segment(sys.argv[1], sys.argv[2])
