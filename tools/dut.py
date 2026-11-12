# This file is part of the ScrewSync project.
# Copyright (c) 2026 Donatas Olsevičius
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

# Install: pip install pyserial

import os

import serial
from serial.tools import list_ports

BAUD = 115200
ESPRESSIF_VID = 0x303A
# Debug-probe VIDs that can share a host with the DUT but never run our
# firmware's console -- excluded from port autodetection.
NON_DUT_VIDS = {0x1366}  # SEGGER


def resolve_port(explicit=None):
    """explicit arg -> $DUT_PORT -> a lone serial port."""
    if explicit:
        return explicit
    if os.environ.get("DUT_PORT"):
        return os.environ["DUT_PORT"]
    ports = list(list_ports.comports())
    espressif = [p.device for p in ports if (p.vid or 0) == ESPRESSIF_VID]
    if len(espressif) == 1:
        return espressif[0]
    usb = [p for p in ports if p.vid and p.vid not in NON_DUT_VIDS]
    if len(usb) == 1:
        return usb[0].device
    raise RuntimeError(
        "Cannot pick a serial port. Pass one explicitly or set $DUT_PORT. Seen: "
        + ", ".join(f"{p.device}({p.vid:#06x})" if p.vid else p.device for p in ports)
    )


def _open(port, baud):
    """Open the port without touching DTR. On the native USB-Serial-JTAG
    peripheral (ESP32-C3/S3), the ROM resets into the bootloader on any DTR
    edge -- pyserial issues that edge on open regardless of the value you
    assign (serialposix.py calls _update_dtr_state() unless dsrdtr=True),
    so dsrdtr=True is what actually skips it, not ser.dtr = False."""
    ser = serial.Serial(baudrate=baud, timeout=0.15, dsrdtr=True)
    ser.port = port
    ser.rts = False
    ser.open()
    return ser
