import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

import pytest
import requests
import serial

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).resolve().parent / "fixtures"
sys.path.insert(0, str(ROOT / "tools"))
from dut import _open, resolve_port  # noqa: E402

TARGET = os.environ["SMOKE_TARGET"]
FIRMWARE = Path(os.environ["SMOKE_FIRMWARE"])
TFT = Path(os.environ["SMOKE_TFT"]) if os.environ.get("SMOKE_TFT") else None
KV = re.compile(r'(\w+)=("[^"]*"|\S+)')


def kv(line):
    return {k: v.strip('"') for k, v in KV.findall(line)}


def git_version():
    return subprocess.run(["git", "describe", "--always", "--dirty"], cwd=ROOT / "main", capture_output=True,
                          text=True, check=True).stdout.strip()


class Dut:
    def __init__(self):
        self.port = resolve_port()
        self.ser = None
        self.buf = b""

    def open(self, timeout=60):
        deadline = time.monotonic() + timeout
        while True:
            try:
                self.ser = _open(self.port, 115200)
                if self.cmd("ping", timeout=2) == "ok pong":
                    return
            except (serial.SerialException, OSError, TimeoutError):
                pass
            self.close()
            if time.monotonic() > deadline:
                raise TimeoutError(f"no ping reply on {self.port}")
            time.sleep(0.5)

    def close(self):
        if self.ser:
            try:
                self.ser.close()
            except (serial.SerialException, OSError):
                pass
        self.ser = None
        self.buf = b""

    def readline(self, deadline):
        while b"\n" not in self.buf:
            if time.monotonic() > deadline:
                return None
            self.buf += self.ser.read(self.ser.in_waiting or 1)
        line, self.buf = self.buf.split(b"\n", 1)
        text = line.decode(errors="replace").strip()
        if text:
            print(f"  dut| {text}")
        return text

    def expect(self, pattern, timeout):
        deadline = time.monotonic() + timeout
        rx = re.compile(pattern)
        while True:
            line = self.readline(deadline)
            if line is None:
                raise TimeoutError(f"no line matching {pattern!r}")
            m = rx.search(line)
            if m:
                return m

    def send(self, line):
        self.ser.reset_input_buffer()
        self.buf = b""
        self.ser.write((line + "\n").encode())
        self.ser.flush()

    def cmd(self, line, timeout=5):
        self.send(line)
        return self.expect(r"^(ok|err)\b.*", timeout).group(0)

    def ok(self, line, timeout=5):
        reply = self.cmd(line, timeout)
        assert reply.startswith("ok"), f"{line}: {reply}"
        return reply

    def info(self):
        return kv(self.ok("info"))

    def reconnect(self, timeout=90):
        self.close()
        time.sleep(2)
        self.open(timeout)

    def wait_rebooted(self, timeout=90):
        self.reconnect(timeout)
        info = self.info()
        assert int(info["uptime"]) < timeout, f"did not reboot: {info}"
        return info

    def wait_confirmed(self, timeout=90):
        deadline = time.monotonic() + timeout
        while self.info()["confirmed"] != "1":
            if time.monotonic() > deadline:
                raise TimeoutError("firmware image not confirmed")
            time.sleep(2)

    def ip(self, timeout=60):
        deadline = time.monotonic() + timeout
        while True:
            ip = kv(self.ok("wifi status")).get("ip", "")
            if ip and ip != "0.0.0.0":
                return ip
            if time.monotonic() > deadline:
                raise TimeoutError("no Wi-Fi IP")
            time.sleep(1)

    def display_connected(self):
        reply = self.cmd("display status")
        if reply == "err busy: flashing":
            return "0"
        assert reply.startswith("ok"), f"display status: {reply}"
        return kv(reply)["connected"]

    def wait_display(self, connected, timeout):
        deadline = time.monotonic() + timeout
        want = "1" if connected else "0"
        while self.display_connected() != want:
            if time.monotonic() > deadline:
                raise TimeoutError(f"display connected!={want}")
            time.sleep(1)

    def make(self, goal):
        self.close()
        try:
            subprocess.run(["make", "--no-print-directory", goal], cwd=ROOT, check=True)
        finally:
            self.open()


@pytest.fixture(scope="session")
def dut():
    d = Dut()
    d.open()
    yield d
    d.close()


@pytest.fixture
def base_url(dut, timeout=30):
    url = f"http://{dut.ip()}"
    deadline = time.monotonic() + timeout
    while True:
        try:
            requests.get(url + "/", timeout=2)
            return url
        except requests.RequestException:
            if time.monotonic() > deadline:
                raise
            time.sleep(1)


class Status:
    def __init__(self, url):
        from websockets.sync.client import connect

        # Unbounded queue keeps answering the board's pings while a test waits for input.
        # The board drops clients that send their own pings.
        self.ws = connect(url.replace("http://", "ws://") + "/ws", max_queue=None, ping_interval=None)

    def latest(self, timeout=5):
        last = None
        while True:
            try:
                msg = json.loads(self.ws.recv(timeout=0 if last else timeout))
            except TimeoutError:
                if last:
                    return last
                raise
            if msg.get("type") == "status":
                last = msg

    def fresh(self, timeout=5):
        self.latest(timeout)
        time.sleep(1)
        return self.latest(timeout)

    def close(self):
        self.ws.close()
