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
"""blast_connect -- repeatedly sends Nextion's "connect" handshake out a
serial port from this computer, for feeding into something else's RX pin
(e.g. the ESP32's) as a known-good, independently-clocked external source.
Does not read or print any reply -- watch the other end's own log.

    pip install pyserial
    python tools/blast_connect.py --port /dev/ttyUSB0
"""

import argparse
import sys
import time

from dut import _open, resolve_port

INTERVAL_S = 0.5


def main():
    ap = argparse.ArgumentParser(description="repeatedly send Nextion's connect handshake")
    ap.add_argument("--port", help="serial port (default: autodetect / $DUT_PORT)")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    port = resolve_port(args.port)
    ser = _open(port, args.baud)
    print(f"blast_connect: sending on {port} @ {args.baud} baud, every {INTERVAL_S}s -- Ctrl-C to stop", file=sys.stderr)

    count = 0
    try:
        while True:
            ser.write(b"connect\xff\xff\xff")
            ser.flush()
            count += 1
            print(f"sent #{count}", file=sys.stderr)
            time.sleep(INTERVAL_S)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()


if __name__ == "__main__":
    main()
