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
"""console -- a plain passive serial monitor for screwsync.

Everything the device prints, printed; anything typed here, sent as a
console line (e.g. `ping`). Unlike `west espressif monitor`/`pio device
monitor`, it opens the port the same way tools/dut.py does -- DTR/RTS held
low, so it doesn't reset the board on connect the way pyserial's defaults
do on the native USB-Serial-JTAG console.

Reconnects automatically if the port disappears (cable unplug, reset, power
cycle) instead of crashing -- start this first, then power-cycle the board,
to catch its earliest boot output.

    pip install pyserial
    python tools/console.py                 # autodetect a lone serial port
    python tools/console.py --port /dev/ttyACM0
    python tools/console.py --tft .build/release/nextion-5in/display.tft   # upload, flash, keep monitoring

Typing `!tft [path]` uploads a display .tft the same way (default
.build/release/nextion-5in/display.tft): the firmware stores it, then flashes the display.
"""

import argparse
import sys
import threading
import time
import zlib
from pathlib import Path

import serial

from dut import BAUD, _open, resolve_port

RECONNECT_POLL_S = 0.2
DEFAULT_TFT = Path(__file__).resolve().parent.parent / ".build" / "release" / "nextion-5in" / "display.tft"
UPLOAD_REPLY_TIMEOUT_S = 10
READY_TIMEOUT_S = 30
PING_INTERVAL_S = 0.5
PONG = "ok pong"
UPLOAD_CHUNK = 4096
UPLOAD_REPLIES = ("ok upload", "err display upload")
FLASH_STARTED = "ok display flash started"
UPLOAD_ERRORS = ("err display upload", "err display flash")

_start = time.monotonic()


def _reply(text, prefixes):
    """The reply from the first prefix on: boot log text without a line end
    can come before it."""
    starts = [i for i in (text.find(p) for p in prefixes) if i >= 0]
    return text[min(starts):] if starts else None


def _stamp():
    m, sec = divmod(int(time.monotonic() - _start), 60)
    return f"[{m:02d}:{sec:02d}]"


def _request(state, line, prefixes, timeout):
    """Sends line; the first reply that has one of prefixes, else None."""
    ser = state["ser"]
    if ser is None:
        return None
    state["reply"] = None
    state["reply_event"].clear()
    state["expect"] = prefixes
    try:
        ser.write(f"{line}\n".encode())
    except (serial.SerialException, OSError):
        return None
    got = state["reply_event"].wait(timeout)
    state["expect"] = None
    return state["reply"] if got else None


def _wait_ready(state):
    """Pings until the firmware answers, so a booting device gets no upload."""
    deadline = time.monotonic() + READY_TIMEOUT_S
    while time.monotonic() < deadline:
        if _request(state, "ping", (PONG,), PING_INTERVAL_S):
            return True
    return False


def _upload_tft(state, path):
    if state["ser"] is None:
        print(f"{_stamp()} console: not connected", file=sys.stderr)
        return False
    try:
        image = Path(path).read_bytes()
    except OSError as e:
        print(f"{_stamp()} console: {e}", file=sys.stderr)
        return False
    if not _wait_ready(state):
        print(f"{_stamp()} console: no reply to ping", file=sys.stderr)
        return False
    reply = _request(state, f"display upload {len(image)} {zlib.crc32(image):08x}", UPLOAD_REPLIES, UPLOAD_REPLY_TIMEOUT_S)
    if reply is None:
        print(f"{_stamp()} console: no reply to display upload", file=sys.stderr)
        return False
    if reply != "ok upload ready":
        return not reply.startswith("err")
    ser = state["ser"]
    print(f"{_stamp()} console: uploading {path} ({len(image)} B)", file=sys.stderr)
    last_pct = -1
    try:
        for off in range(0, len(image), UPLOAD_CHUNK):
            ser.write(image[off:off + UPLOAD_CHUNK])
            pct = (off + UPLOAD_CHUNK) * 100 // len(image)
            if pct // 10 != last_pct // 10:
                last_pct = pct
                print(f"{_stamp()} console: {min(pct, 100)}%", file=sys.stderr)
        ser.flush()
    except (serial.SerialException, OSError) as e:
        print(f"{_stamp()} console: upload failed: {e}", file=sys.stderr)
        return False
    return True


def _upload_then_flag(state, path):
    if not _upload_tft(state, path):
        state["upload_failed"] = True


def _writer(state):
    # stdin EOF (piped/redirected) is not "stop" -- only the reader loop is.
    # state["ser"] is swapped out from under us on reconnect, so re-read it
    # per line rather than capturing one serial.Serial at thread start.
    for line in sys.stdin:
        if line.startswith("!tft"):
            _upload_tft(state, line[4:].strip() or DEFAULT_TFT)
            continue
        ser = state["ser"]
        if ser is None:
            continue
        try:
            ser.write((line.rstrip("\n") + "\n").encode())
        except (serial.SerialException, OSError):
            pass


def main():
    ap = argparse.ArgumentParser(description="plain serial monitor for screwsync")
    ap.add_argument("--port", help="serial port (default: autodetect / $DUT_PORT)")
    ap.add_argument("--baud", type=int, default=BAUD)
    ap.add_argument("--tft", help="upload this display .tft once connected, then keep monitoring")
    ap.add_argument("--exit-after-upload", action="store_true", help="with --tft, exit once the display flash has started")
    args = ap.parse_args()

    print(f"{_stamp()} console: waiting for a port @ {args.baud} -- Ctrl-C to stop", file=sys.stderr)

    state = {"ser": None, "reply": None, "reply_event": threading.Event(), "expect": None, "upload_failed": False}
    pending_tft = args.tft
    threading.Thread(target=_writer, args=(state,), daemon=True).start()

    buf = b""
    done = False
    exit_code = 0
    try:
        while not done:
            if state["ser"] is None:
                try:
                    port = resolve_port(args.port)
                    ser = _open(port, args.baud)
                except (serial.SerialException, OSError, RuntimeError):
                    time.sleep(RECONNECT_POLL_S)
                    continue
                time.sleep(0.3)
                ser.reset_input_buffer()  # drop whatever piled up before we started reading
                buf = b""
                print(f"{_stamp()} console: connected to {port}", file=sys.stderr)
                state["ser"] = ser
                if pending_tft:
                    threading.Thread(target=_upload_then_flag, args=(state, pending_tft), daemon=True).start()
                    pending_tft = None

            try:
                chunk = state["ser"].read(256)
            except (serial.SerialException, OSError):
                print(f"{_stamp()} console: disconnected, waiting to reconnect...", file=sys.stderr)
                try:
                    state["ser"].close()
                except Exception:
                    pass
                state["ser"] = None
                time.sleep(RECONNECT_POLL_S)
                continue

            if args.exit_after_upload and state["upload_failed"]:
                exit_code = 1
                break
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                head, buf = buf.split(b"\n", 1)
                text = head.decode(errors="replace").rstrip("\r")
                expect = state["expect"]
                reply = _reply(text, expect) if expect else None
                if reply:
                    state["reply"] = reply
                    state["reply_event"].set()
                print(_stamp(), text, flush=True)
                if args.exit_after_upload and args.tft:
                    if _reply(text, (FLASH_STARTED,)):
                        done = True
                        break
                    if _reply(text, UPLOAD_ERRORS):
                        exit_code = 1
                        done = True
                        break
    except KeyboardInterrupt:
        pass
    finally:
        if state["ser"] is not None:
            state["ser"].close()
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
