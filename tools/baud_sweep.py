#!/usr/bin/env python3
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
"""baud_sweep -- probes a Nextion display's "connect" handshake across a
range of baud rates from this computer directly (e.g. via an FTDI USB-TTL
adapter), independent of the ESP32/Zephyr firmware entirely.

Sends "connect" + 0xFF 0xFF 0xFF at each baud, listens briefly, and prints
whatever comes back as hex + a printable-ASCII rendering. A real reply
contains "comok" somewhere in it.

    pip install pyserial
    python tools/baud_sweep.py --port /dev/ttyUSB0
"""

import argparse
import sys
import time

import serial

from dut import _open, resolve_port

BAUDS = [2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 250000, 256000, 512000, 921600]
LISTEN_S = 0.5


def printable(data: bytes) -> str:
    return "".join(chr(b) if 32 <= b < 127 else "." for b in data)


def probe(port: str, baud: int) -> bytes:
    ser = _open(port, baud)
    try:
        ser.reset_input_buffer()
        ser.write(b"connect\xff\xff\xff")
        ser.flush()
        deadline = time.monotonic() + LISTEN_S
        data = b""
        while time.monotonic() < deadline:
            chunk = ser.read(256)
            if chunk:
                data += chunk
        return data
    finally:
        ser.close()


def main():
    ap = argparse.ArgumentParser(description="sweep baud rates against a Nextion display's connect handshake")
    ap.add_argument("--port", help="serial port (default: autodetect / $DUT_PORT)")
    ap.add_argument("--repeat", type=int, default=1, help="sweep the whole list this many times (0 = forever)")
    args = ap.parse_args()

    port = resolve_port(args.port)
    print(f"baud_sweep: using {port}", file=sys.stderr)

    count = 0
    try:
        while args.repeat == 0 or count < args.repeat:
            for baud in BAUDS:
                try:
                    data = probe(port, baud)
                except (serial.SerialException, OSError) as e:
                    print(f"{baud:>7} -> error: {e}")
                    continue
                if data:
                    tag = " <-- comok!" if b"comok" in data else ""
                    print(f"{baud:>7} -> {len(data)} bytes: {data.hex(' ')}  |{printable(data)}|{tag}")
                else:
                    print(f"{baud:>7} -> 0 bytes")
            count += 1
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
