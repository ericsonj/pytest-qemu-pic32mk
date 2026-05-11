"""
Startup module — provides the _reset entry point and software
interrupt dispatcher for QEMU emulation.

irq_dispatch.S provides the software vector dispatcher that reads
EVIC INTSTAT and jumps to the correct handler. It is now included
in the build (previously excluded when TARGET's interrupts_a.S was
the only vector provider).
"""

from pm import mk

mk(incs=[])