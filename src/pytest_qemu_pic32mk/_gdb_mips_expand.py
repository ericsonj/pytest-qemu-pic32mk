"""GDB Python hook — expand DWARF lazy-load on every new objfile.

Bundled as package data so gdb_snapshot.py and QEMUProcess can reference it
with a stable path regardless of where the library is installed.

Loaded via ``source <path>`` in GDB's -batch / setupCommands.
"""
import gdb


def _on_new_objfile(event):
    gdb.execute("maintenance expand-symtabs", to_string=True)


gdb.events.new_objfile.connect(_on_new_objfile)
