"""Host-side bq79606 UART mock for QEMU UART1 (BMS UART).

Protocol reference: BQ79606A-Q1 datasheet §8.5.1 (SLUSDQ4 — April 2019)

Command frames (host → device), bit7 of INIT byte = 1:
  0x80  Single Device READ   [INIT][DEV][REG_H][REG_L][N-1][CRC_H][CRC_L]      7 bytes
  0x90  Single Device WRITE  [INIT][DEV][REG_H][REG_L][DATA×n][CRC_H][CRC_L]   6+n bytes
  0xA0  Stack READ           [INIT][REG_H][REG_L][N-1][CRC_H][CRC_L]            6 bytes
  0xB0  Stack WRITE          [INIT][REG_H][REG_L][DATA×n][CRC_H][CRC_L]         5+n bytes
  0xC0  Broadcast READ       [INIT][REG_H][REG_L][N-1][CRC_H][CRC_L]            6 bytes
  0xD0  Broadcast WRITE      [INIT][REG_H][REG_L][DATA×n][CRC_H][CRC_L]         5+n bytes
  0xE0  Broadcast WRITE Rev  [INIT][REG_H][REG_L][DATA×1][CRC_H][CRC_L]         6 bytes

Response frames (device → host, bit7 of INIT byte = 0):
  [n-1][DEV][REG_H][REG_L][DATA×n][CRC_H][CRC_L]

DATA_SIZE field: bits [2:0] of INIT byte encode (n_data - 1) for write frames.
For read command frames DATA_SIZE is always 0b000 (one data byte = the read count).

CRC: CRC-16/IBM (poly=0xA001, init=0xFFFF) then bytes swapped — matches
     CalculateCRCForBMS() in firmware.
"""

from __future__ import annotations

import socket
import struct
import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Optional

# ── Register addresses (§8.6 Register Maps) ────────────────────────────────────
_REG_CONFIG         = 0x0001   # Device Configuration (STACK_DEV, TOP_STACK)
_REG_COMM_CTRL      = 0x0020   # Communication Control (BAUD, UARTTX_EN, NFAULT_EN)
_REG_DEVADD_USR     = 0x0104   # Programmable Device Stack Address
_REG_CONTROL1       = 0x0105   # Device Control (ADD_WRITE_EN, DIR_SEL, SOFT_RESET)
_REG_CONTROL2       = 0x0106   # Function Enable (TSREF_EN, OTUT_EN, OVUV_EN, CELL_ACD_GO)
_REG_CELL_ADC_CTRL  = 0x0109   # Cell ADC Control (per-cell enable bits)

_REG_SYS_FAULT1_FLT_RST = 0x013B  # Write 1 to bit → clear that SYS_FAULT1 bit (§8.6.1.261)

_REG_SYS_FAULT1     = 0x0201   # System Fault 1 — bit0=DRST (set on every power-up)
_REG_SYS_FAULT2     = 0x0202   # System Fault 2
_REG_SYS_FAULT3     = 0x0203   # IC System Fault 3
_REG_DEV_STAT       = 0x0204   # Device Status — bits[3:0] = ADC channel conversion ready
_REG_FAULT_SUMMARY  = 0x0206   # Fault Summary

# Burst cell-voltage read: VCELL1_HF=0x0207, VCELL1_LF=0x0208, ..., VCELL6_HF=0x0211, VCELL6_LF=0x0212
# Firmware issues BMSSingleDeviceReadBigData(dev, 0x0207, 2*n_strings, buf).
_REG_VCELL1_HF      = 0x0207   # Cell 1 Voltage High Byte (Low-Pass Filtered)

# Burst NTC read: AUX_GPIO1H=0x022D, AUX_GPIO1L=0x022E, ..., AUX_GPIO6H=0x0237
# Firmware issues BMSSingleDeviceReadBigData(dev, 0x022D, 2*n_ntcs, buf).
_REG_AUX_GPIO1H     = 0x022D   # GPIO1 (NTC1) Voltage High Byte (Corrected)

# Fault status registers — firmware reads these during fault monitoring
_REG_UV_FAULT       = 0x0291   # UV Comparator Fault Status (per-cell bits [5:0])
_REG_OV_FAULT       = 0x0292   # OV Comparator Fault Status (per-cell bits [5:0])
_REG_UT_FAULT       = 0x0293   # UT Comparator Fault Status (per-NTC bits [5:0])
_REG_OT_FAULT       = 0x0294   # OT Comparator Fault Status (per-NTC bits [5:0])

# ── Voltage conversion (§8.3) ──────────────────────────────────────────────────
# BQ_TO_mV(raw) = (raw * COMP2_TO_MV_Q16) >> 16   COMP2_TO_MV = 0.1907349
_COMP2_TO_MV_Q16 = int(0.1907349 * 65535)  # = 12499

# ── Limits / sentinels ─────────────────────────────────────────────────────────
_MAX_DEVICES    = 128
_MAX_CELLS      = 8
_MAX_NTCS       = 8
_DEV_STAT_READY = 0x0F   # all four ADC conversion channels done

# dev_addr sentinels stored in WriteRecord for stack/broadcast commands
ADDR_STACK     = 0xFE
ADDR_BROADCAST = 0xFF

# ── FAULT_SUMMARY bits (0x0206) — firmware only reads detail regs when these are set ──
_BMS_CELL_OVUV = 0x02   # bit 1: OV or UV fault on any cell
_BMS_GPIO_OTUT = 0x04   # bit 2: OT or UT fault on any GPIO/NTC

# ── Default register values matching datasheet hardware reset values ────────────
_REGISTER_DEFAULTS: dict[int, int] = {
    _REG_CONFIG:        0x00,
    _REG_COMM_CTRL:     0x34,   # BAUD=250 kbps, UARTTX_EN, NFAULT_EN (OTP 0x3C → post-reset 0x34)
    _REG_CONTROL1:      0x00,
    _REG_CONTROL2:      0x00,
    _REG_DEVADD_USR:    0x00,
    _REG_SYS_FAULT1:    0x01,   # DRST bit set on every digital reset (datasheet reset value = 0x01)
    _REG_SYS_FAULT2:    0x00,
    _REG_SYS_FAULT3:    0x00,
    _REG_DEV_STAT:      _DEV_STAT_READY,  # mock always signals conversions ready
    _REG_FAULT_SUMMARY: 0x00,
    _REG_UV_FAULT:      0x00,
    _REG_OV_FAULT:      0x00,
    _REG_UT_FAULT:      0x00,
    _REG_OT_FAULT:      0x00,
}


# ── CRC ───────────────────────────────────────────────────────────────────────

def _crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def _bms_crc(data: bytes) -> bytes:
    """CRC as transmitted: CRC-16/IBM then byte-swapped (per CalculateCRCForBMS)."""
    crc = _crc16_modbus(data)
    swapped = ((crc << 8) | (crc >> 8)) & 0xFFFF
    return struct.pack(">H", swapped)


def _crc_valid(buf: bytearray, frame_len: int) -> bool:
    """Return True if the last 2 bytes of buf[:frame_len] are the correct CRC."""
    payload = bytes(buf[:frame_len - 2])
    expected = _bms_crc(payload)
    return bytes(buf[frame_len - 2:frame_len]) == expected


# ── Voltage helpers ────────────────────────────────────────────────────────────

def _mv_to_raw(mv: int) -> int:
    """Inverse of BQ_TO_mV: millivolts → raw 16-bit ADC register value."""
    return min(0xFFFF, (mv * 65536 + _COMP2_TO_MV_Q16 - 1) // _COMP2_TO_MV_Q16)


# ── Write record ───────────────────────────────────────────────────────────────

@dataclass
class WriteRecord:
    """One captured write command received from the firmware.

    dev_addr: specific device address, or ADDR_STACK (0xFE) / ADDR_BROADCAST (0xFF).
    reg_addr: starting register address.
    data:     bytes written.
    timestamp: monotonic time of receipt.
    """
    dev_addr:  int
    reg_addr:  int
    data:      bytes
    timestamp: float = field(default_factory=time.monotonic)


# ── Main mock class ────────────────────────────────────────────────────────────

class BMSMock:  # pylint: disable=too-many-instance-attributes
    """Host-side bq79606 UART mock. Connects to QEMU UART1 Unix socket as client.

    Implements the full command/response protocol from the bq79606 datasheet:
    - Single Device READ → one response frame
    - Stack READ         → n_devices response frames (highest address first)
    - Broadcast READ     → n_devices response frames (highest address first)
    - All writes are silently consumed and tracked in the register store

    ── Dynamic value setters (thread-safe, callable while mock is running) ──────
        set_cell_voltage(device, cell, voltage_mv)
        set_all_cell_voltages(voltage_mv)
        set_temperature(device, ntc, temperature_raw)
        set_all_temperatures(temperature_raw)

    ── Fault injection (call before or during a test) ───────────────────────────
        inject_register(device, reg_addr, value)   — set any register byte
        set_sys_fault1(device, bits)               — SYS_FAULT1 0x0201
        set_ov_fault(device, cell_mask)            — OV_FAULT   0x0292
        set_uv_fault(device, cell_mask)            — UV_FAULT   0x0291
        set_ot_fault(device, ntc_mask)             — OT_FAULT   0x0294
        set_ut_fault(device, ntc_mask)             — UT_FAULT   0x0293
        clear_faults(device)                       — clear all fault registers

    ── Write capture (for test assertions on firmware configuration) ─────────────
        on_write(callback)                         — cb(dev_addr, reg_addr, data)
        get_writes(reg_addr=None)                  — list[WriteRecord]
        clear_write_log()
    """

    def __init__(
        self,
        sock_path: str,
        n_devices: int,
        cell_voltage_mv: int = 3450,
        temperature_raw: int = 0,
        verbose: bool = False,
    ) -> None:
        self._sock_path = sock_path
        self._n_devices = n_devices
        self._verbose = verbose
        self._lock = threading.Lock()

        default_raw = _mv_to_raw(cell_voltage_mv)
        self._voltages: list[list[int]] = [
            [default_raw] * _MAX_CELLS for _ in range(_MAX_DEVICES)
        ]
        self._temperatures: list[list[int]] = [
            [temperature_raw] * _MAX_NTCS for _ in range(_MAX_DEVICES)
        ]

        # Per-device register store: _registers[dev_idx][reg_addr] = byte_value
        # dev_idx = dev_addr - 1 (devices are 1-based; bridge/base = dev_addr 0)
        self._registers: list[dict[int, int]] = [
            dict(_REGISTER_DEFAULTS) for _ in range(_MAX_DEVICES)
        ]

        self._write_log: list[WriteRecord] = []
        self._write_callbacks: list[Callable[[int, int, bytes], None]] = []

        self._sock: Optional[socket.socket] = None
        self._thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()
        self._silent: bool = False

    # ── Dynamic value setters ──────────────────────────────────────────────────

    def set_cell_voltage(self, device: int, cell: int, voltage_mv: int) -> None:
        with self._lock:
            self._voltages[device][cell] = _mv_to_raw(voltage_mv)

    def set_all_cell_voltages(self, voltage_mv: int) -> None:
        raw = _mv_to_raw(voltage_mv)
        with self._lock:
            for dev in range(_MAX_DEVICES):
                for cell in range(_MAX_CELLS):
                    self._voltages[dev][cell] = raw

    def set_temperature(self, device: int, ntc: int, temperature_raw: int) -> None:
        """Set raw NTC register value for one sensor (see BMSConversionToTempdC LUT)."""
        with self._lock:
            self._temperatures[device][ntc] = temperature_raw

    def set_all_temperatures(self, temperature_raw: int) -> None:
        with self._lock:
            for dev in range(_MAX_DEVICES):
                for ntc in range(_MAX_NTCS):
                    self._temperatures[dev][ntc] = temperature_raw

    # ── Fault injection ────────────────────────────────────────────────────────

    def inject_register(self, device: int, reg_addr: int, value: int) -> None:
        """Overwrite any single register byte in a device's register store.

        device: 0-indexed (device 0 = firmware address 1).
        Does NOT update FAULT_SUMMARY — use the typed fault setters for that.
        """
        with self._lock:
            self._registers[device][reg_addr] = value & 0xFF

    def set_sys_fault1(self, device: int, bits: int) -> None:
        """Set SYS_FAULT1 (0x0201). Common bits: bit0=DRST, bit5=TWARN."""
        self.inject_register(device, _REG_SYS_FAULT1, bits)

    def set_ov_fault(self, device: int, cell_mask: int) -> None:
        """Set OV_FAULT (0x0292) and update FAULT_SUMMARY BMS_CELL_OVUV bit.

        Simulates what the hardware OV comparator circuit does when a cell voltage
        rises above the OV threshold (OVTHRESH register). On real hardware this
        happens autonomously — independent of ADC conversions.

        device:    0-indexed device number (device 0 = stack address 1).
        cell_mask: bitmask of cells in over-voltage condition.
                   bit 0 = cell 1 (lowest in stack)
                   bit 1 = cell 2
                   …
                   bit 5 = cell 6 (highest in stack)
                   Pass 0 to clear the OV fault on all cells.

        Side effects:
          - Writes cell_mask into OV_FAULT (0x0292) for the device.
          - Re-evaluates FAULT_SUMMARY[CELL_OVUV] (bit 1):
              set   if (OV_FAULT | UV_FAULT) != 0  (either fault active)
              clear if (OV_FAULT | UV_FAULT) == 0  (both cleared)

        Examples:
            # Cell 1 over-voltage on device 0 (stack addr 1)
            mock.set_ov_fault(device=0, cell_mask=0b000001)

            # Cells 2 and 4 over-voltage on device 1 (stack addr 2)
            mock.set_ov_fault(device=1, cell_mask=0b001010)

            # All 6 cells over-voltage — overcharged pack scenario
            mock.set_ov_fault(device=0, cell_mask=0b111111)  # 0x3F

            # Clear OV fault on device 0 (UV_FAULT still clears CELL_OVUV if also 0)
            mock.set_ov_fault(device=0, cell_mask=0)

        PDF ref: §8.6 OV_FAULT register (0x0292), FAULT_SUMMARY (0x0206) bit 1.
        """
        with self._lock:
            self._registers[device][_REG_OV_FAULT] = cell_mask & 0xFF
            self._update_fault_summary(device, _BMS_CELL_OVUV,
                                       cell_mask | self._registers[device].get(_REG_UV_FAULT, 0))

    def set_uv_fault(self, device: int, cell_mask: int) -> None:
        """Set UV_FAULT (0x0291) and update FAULT_SUMMARY BMS_CELL_OVUV bit.

        Simulates what the hardware UV comparator circuit does when a cell voltage
        drops below the UV threshold (UVTHRESH register). On real hardware this
        happens autonomously — independent of ADC conversions.

        device:    0-indexed device number (device 0 = stack address 1).
        cell_mask: bitmask of cells in under-voltage condition.
                   bit 0 = cell 1 (lowest in stack)
                   bit 1 = cell 2
                   …
                   bit 5 = cell 6 (highest in stack)
                   Pass 0 to clear the UV fault on all cells.

        Side effects:
          - Writes cell_mask into UV_FAULT (0x0291) for the device.
          - Re-evaluates FAULT_SUMMARY[CELL_OVUV] (bit 1):
              set   if (UV_FAULT | OV_FAULT) != 0  (either fault active)
              clear if (UV_FAULT | OV_FAULT) == 0  (both cleared)

        Examples:
            # Cell 1 under-voltage on device 0 (stack addr 1)
            mock.set_uv_fault(device=0, cell_mask=0b000001)

            # Cells 1, 3, and 5 under-voltage on device 1 (stack addr 2)
            mock.set_uv_fault(device=1, cell_mask=0b010101)

            # All 6 cells under-voltage — discharged pack scenario
            mock.set_uv_fault(device=0, cell_mask=0b111111)  # 0x3F

            # Clear UV fault on device 0 (OV_FAULT still clears CELL_OVUV if also 0)
            mock.set_uv_fault(device=0, cell_mask=0)

        PDF ref: §8.6 UV_FAULT register (0x0291), FAULT_SUMMARY (0x0206) bit 1.
        """
        with self._lock:
            self._registers[device][_REG_UV_FAULT] = cell_mask & 0xFF
            self._update_fault_summary(device, _BMS_CELL_OVUV,
                                       cell_mask | self._registers[device].get(_REG_OV_FAULT, 0))

    def set_ot_fault(self, device: int, ntc_mask: int) -> None:
        """Set OT_FAULT (0x0294) and update FAULT_SUMMARY BMS_GPIO_OTUT bit.

        bit0=GPIO1 … bit5=GPIO6. Pass 0 to clear.
        """
        with self._lock:
            self._registers[device][_REG_OT_FAULT] = ntc_mask & 0xFF
            self._update_fault_summary(device, _BMS_GPIO_OTUT,
                                       ntc_mask | self._registers[device].get(_REG_UT_FAULT, 0))

    def set_ut_fault(self, device: int, ntc_mask: int) -> None:
        """Set UT_FAULT (0x0293) and update FAULT_SUMMARY BMS_GPIO_OTUT bit.

        bit0=GPIO1 … bit5=GPIO6. Pass 0 to clear.
        """
        with self._lock:
            self._registers[device][_REG_UT_FAULT] = ntc_mask & 0xFF
            self._update_fault_summary(device, _BMS_GPIO_OTUT,
                                       ntc_mask | self._registers[device].get(_REG_OT_FAULT, 0))

    def clear_faults(self, device: int) -> None:
        """Clear all fault registers and FAULT_SUMMARY for a device (0-indexed)."""
        with self._lock:
            for reg in (
                _REG_SYS_FAULT1, _REG_SYS_FAULT2, _REG_SYS_FAULT3,
                _REG_FAULT_SUMMARY,
                _REG_UV_FAULT, _REG_OV_FAULT, _REG_UT_FAULT, _REG_OT_FAULT,
            ):
                self._registers[device][reg] = 0x00

    # ── Write capture ──────────────────────────────────────────────────────────

    def on_write(self, callback: Callable[[int, int, bytes], None]) -> None:
        """Register callback invoked on every write: cb(dev_addr, reg_addr, data).

        dev_addr is ADDR_STACK or ADDR_BROADCAST for multi-device writes.
        """
        self._write_callbacks.append(callback)

    def get_writes(self, reg_addr: Optional[int] = None) -> list[WriteRecord]:
        """Return captured write records, optionally filtered by register address."""
        with self._lock:
            log = list(self._write_log)
        if reg_addr is not None:
            log = [w for w in log if w.reg_addr == reg_addr]
        return log

    def clear_write_log(self) -> None:
        with self._lock:
            self._write_log.clear()

    def set_silent(self, silent: bool) -> None:
        """Stop (or resume) sending responses to any read command.

        When silent, incoming frames are still consumed but no response is sent.
        This causes the firmware UART to time out on reads, which maps to
        BMS_ERROR_MESSAGE returns — same observable effect as a disconnected device.
        Used in TC-C1 to trigger FAULT_COMM_LOST_BMS via direction-change failure.
        """
        with self._lock:
            self._silent = silent

    # ── Lifecycle ──────────────────────────────────────────────────────────────

    def start(self, connect_timeout: float = 10.0) -> None:
        """Connect to QEMU socket and start the RX dispatch thread."""
        self._stop_event.clear()
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        deadline = time.monotonic() + connect_timeout
        while time.monotonic() < deadline:
            try:
                self._sock.connect(self._sock_path)
                break
            except (FileNotFoundError, ConnectionRefusedError):
                time.sleep(0.05)
        else:
            raise TimeoutError(
                f"BMS UART socket {self._sock_path!r} not ready after {connect_timeout}s"
            )
        self._thread = threading.Thread(target=self._run, name="bms-mock-rx", daemon=True)
        self._thread.start()
        print(f"[BMS_MOCK] connected to {self._sock_path} ({self._n_devices} devices)", flush=True)

    def stop(self) -> None:
        self._stop_event.set()
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
        if self._thread:
            self._thread.join(timeout=2.0)
        print("[BMS_MOCK] stopped", flush=True)

    # ── Internal RX loop ───────────────────────────────────────────────────────

    def _run(self) -> None:
        buf = bytearray()
        assert self._sock is not None
        self._sock.settimeout(0.2)
        while not self._stop_event.is_set():
            try:
                chunk = self._sock.recv(256)
                if not chunk:
                    break
                buf.extend(chunk)
            except socket.timeout:
                continue
            except OSError:
                break
            while True:
                consumed = self._try_consume(buf)
                if consumed == 0:
                    break
                del buf[:consumed]

    def _try_consume(self, buf: bytearray) -> int:  # pylint: disable=too-many-return-statements
        """Parse one frame from buf. Returns bytes consumed, or 0 if incomplete."""
        if not buf:
            return 0

        init = buf[0]
        frame_type = init & 0xF0
        # DATA_SIZE bits [2:0]: number of data bytes = value + 1 (for write frames)
        n_data = (init & 0x07) + 1

        if frame_type == 0x80:                        # ── Single Device READ
            if len(buf) < 7:
                return 0
            if not _crc_valid(buf, 7):
                return 7
            dev_addr = buf[1]
            reg_addr = (buf[2] << 8) | buf[3]
            n_req    = buf[4] + 1                     # data byte encodes n_registers - 1
            self._handle_read(dev_addr, reg_addr, n_req)
            return 7

        if frame_type == 0x90:                        # ── Single Device WRITE
            frame_len = 6 + n_data
            if len(buf) < frame_len:
                return 0
            if not _crc_valid(buf, frame_len):
                return frame_len
            dev_addr = buf[1]
            reg_addr = (buf[2] << 8) | buf[3]
            self._record_write(dev_addr, reg_addr, bytes(buf[4:4 + n_data]))
            return frame_len

        if frame_type == 0xA0:                        # ── Stack READ
            if len(buf) < 6:
                return 0
            if not _crc_valid(buf, 6):
                return 6
            reg_addr = (buf[1] << 8) | buf[2]
            n_req    = buf[3] + 1
            self._handle_stack_read(reg_addr, n_req)
            return 6

        if frame_type == 0xB0:                        # ── Stack WRITE
            frame_len = 5 + n_data
            if len(buf) < frame_len:
                return 0
            if not _crc_valid(buf, frame_len):
                return frame_len
            reg_addr = (buf[1] << 8) | buf[2]
            self._record_write(ADDR_STACK, reg_addr, bytes(buf[3:3 + n_data]))
            return frame_len

        if frame_type == 0xC0:                        # ── Broadcast READ
            if len(buf) < 6:
                return 0
            if not _crc_valid(buf, 6):
                return 6
            reg_addr = (buf[1] << 8) | buf[2]
            n_req    = buf[3] + 1
            self._handle_broadcast_read(reg_addr, n_req)
            return 6

        if frame_type == 0xD0:                        # ── Broadcast WRITE
            frame_len = 5 + n_data
            if len(buf) < frame_len:
                return 0
            if not _crc_valid(buf, frame_len):
                return frame_len
            reg_addr = (buf[1] << 8) | buf[2]
            self._record_write(ADDR_BROADCAST, reg_addr, bytes(buf[3:3 + n_data]))
            return frame_len

        if frame_type == 0xE0:                        # ── Broadcast WRITE Reverse Direction
            if len(buf) < 6:
                return 0
            if not _crc_valid(buf, 6):
                return 6
            reg_addr = (buf[1] << 8) | buf[2]
            self._record_write(ADDR_BROADCAST, reg_addr, bytes(buf[3:4]))
            return 6

        # Unknown byte — drop one byte and re-sync
        print(f"[BMS_MOCK] unknown init byte 0x{init:02X}, resyncing", flush=True)
        return 1

    # ── Read handlers ──────────────────────────────────────────────────────────

    def _handle_read(self, dev_addr: int, reg_addr: int, n_data: int) -> None:
        """Single Device READ: send one response frame."""
        if self._silent:
            return
        dev_idx = dev_addr - 1
        if dev_addr == 0 or dev_idx >= self._n_devices:
            data = bytes(n_data)
        else:
            data = self._compute_read_data(dev_idx, dev_addr, reg_addr, n_data)
        self._send_response(dev_addr, reg_addr, data)

    def _handle_stack_read(self, reg_addr: int, n_data: int) -> None:
        """Stack READ: respond for every stack device, highest address first (§8.5.1.2.3)."""
        if self._silent:
            return
        for dev_addr in range(self._n_devices, 0, -1):
            dev_idx = dev_addr - 1
            data = self._compute_read_data(dev_idx, dev_addr, reg_addr, n_data)
            self._send_response(dev_addr, reg_addr, data)

    def _handle_broadcast_read(self, reg_addr: int, n_data: int) -> None:
        """Broadcast READ: stack devices respond highest-first, then base device (addr 0)."""
        if self._silent:
            return
        for dev_addr in range(self._n_devices, 0, -1):
            dev_idx = dev_addr - 1
            data = self._compute_read_data(dev_idx, dev_addr, reg_addr, n_data)
            self._send_response(dev_addr, reg_addr, data)
        # Base/bridge device (address 0) — not a measurement device, returns zeros
        self._send_response(0, reg_addr, bytes(n_data))

    def _compute_read_data(
        self, dev_idx: int, dev_addr: int, reg_addr: int, n_data: int
    ) -> bytes:
        """Build the response bytes for a register read request.

        Special-cased registers return computed values; all others come from the
        per-device register store (unknown addresses return 0x00 per §8.5.1.2.3).
        """
        if reg_addr == _REG_DEVADD_USR:
            # Always echo back the device's own address — used by CheckStackAddress()
            # in bms_init.c to verify that the daisy-chain addressing completed.
            return bytes([dev_addr] * n_data)

        if reg_addr == _REG_VCELL1_HF:
            # Burst: VCELL1_HF/LF, VCELL2_HF/LF, … — big-endian 16-bit per cell
            n_cells = n_data // 2
            with self._lock:
                cells = [
                    self._voltages[dev_idx][c] if c < _MAX_CELLS else 0
                    for c in range(n_cells)
                ]
            return b"".join(struct.pack(">H", v) for v in cells)

        if reg_addr == _REG_AUX_GPIO1H:
            # Burst: AUX_GPIO1H/L, AUX_GPIO2H/L, … — big-endian 16-bit per NTC
            n_ntcs = n_data // 2
            with self._lock:
                ntcs = [
                    self._temperatures[dev_idx][n] if n < _MAX_NTCS else 0
                    for n in range(n_ntcs)
                ]
            return b"".join(struct.pack(">H", v) for v in ntcs)

        # Generic: read consecutive bytes from register store (0x00 for unmapped)
        with self._lock:
            return bytes(
                self._registers[dev_idx].get(reg_addr + i, 0x00)
                for i in range(n_data)
            )

    # ── Write handler ──────────────────────────────────────────────────────────

    def _record_write(self, dev_addr: int, reg_addr: int, data: bytes) -> None:
        """Update register store and write log for any incoming write command."""
        record = WriteRecord(dev_addr, reg_addr, data)

        with self._lock:
            if dev_addr == ADDR_STACK:
                for dev_idx in range(self._n_devices):
                    self._store_bytes(dev_idx, reg_addr, data)
            elif dev_addr == ADDR_BROADCAST:
                if reg_addr == _REG_DEVADD_USR:
                    # Daisy-chain consume: first device with ADD_WRITE_EN=1 stores the
                    # address and clears its own ADD_WRITE_EN — subsequent devices never
                    # receive this write. §8.5.1.3.2 (p.62) Automatic Stack Addressing.
                    for dev_idx in range(self._n_devices):
                        ctrl1 = self._registers[dev_idx].get(_REG_CONTROL1, 0)
                        if ctrl1 & 0x01:  # ADD_WRITE_EN = bit 0
                            self._store_bytes(dev_idx, reg_addr, data)
                            self._store_bytes(dev_idx, _REG_CONTROL1, bytes([ctrl1 & 0xFE]))
                            break  # consumed — stop forwarding to subsequent devices
                else:
                    for dev_idx in range(self._n_devices):
                        self._store_bytes(dev_idx, reg_addr, data)
            elif 0 < dev_addr <= _MAX_DEVICES:
                dev_idx = dev_addr - 1
                if reg_addr == _REG_SYS_FAULT1_FLT_RST:
                    # Mock simplification: clear all fault registers for this device.
                    # Real hardware: written 1-bits clear the matching SYS_FAULT1 bits;
                    # OV/UV/OT/UT faults are comparator outputs with no dedicated RST
                    # register — they clear when the physical condition resolves.
                    for reg in (
                        _REG_SYS_FAULT1, _REG_SYS_FAULT2, _REG_SYS_FAULT3,
                        _REG_FAULT_SUMMARY,
                        _REG_UV_FAULT, _REG_OV_FAULT, _REG_UT_FAULT, _REG_OT_FAULT,
                    ):
                        self._registers[dev_idx][reg] = 0x00
                else:
                    self._store_bytes(dev_idx, reg_addr, data)
            self._write_log.append(record)

        for cb in self._write_callbacks:
            try:
                cb(dev_addr, reg_addr, data)
            except Exception as exc:  # noqa: BLE001
                print(f"[BMS_MOCK] write callback error: {exc}", flush=True)

        if self._verbose:
            tag = {ADDR_STACK: "STACK", ADDR_BROADCAST: "BCAST"}.get(
                dev_addr, f"0x{dev_addr:02X}"
            )
            print(
                f"[BMS_MOCK] WRITE dev={tag} reg=0x{reg_addr:04X} data={data.hex()}",
                flush=True,
            )

    def _update_fault_summary(self, device: int, summary_bit: int, active: int) -> None:
        """Set or clear one FAULT_SUMMARY bit. Caller must hold self._lock."""
        reg = self._registers[device]
        cur = reg.get(_REG_FAULT_SUMMARY, 0)
        reg[_REG_FAULT_SUMMARY] = (cur | summary_bit) if active else (cur & ~summary_bit & 0xFF)

    def _store_bytes(self, dev_idx: int, reg_addr: int, data: bytes) -> None:
        """Write bytes into _registers[dev_idx]. Caller must hold self._lock."""
        regs = self._registers[dev_idx]
        for i, b in enumerate(data):
            regs[reg_addr + i] = b

    # ── Response sender ────────────────────────────────────────────────────────

    def _send_response(self, dev_addr: int, reg_addr: int, data: bytes) -> None:
        n = len(data)
        header = bytes([n - 1, dev_addr, (reg_addr >> 8) & 0xFF, reg_addr & 0xFF])
        payload = header + data
        frame = payload + _bms_crc(payload)
        try:
            assert self._sock is not None
            self._sock.sendall(frame)
        except OSError:
            pass
