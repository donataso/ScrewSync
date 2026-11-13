import json

import requests
from websockets.sync.client import connect

from conftest import Status


def test_index(base_url):
    r = requests.get(base_url + "/", timeout=10)
    assert r.status_code == 200
    assert "<html" in r.text.lower()


def test_log_has_no_errors(base_url):
    r = requests.get(base_url + "/log", timeout=10)
    assert r.status_code == 200
    errors = [line for line in r.text.splitlines() if "<err>" in line]
    assert not errors, "\n".join(errors)


def test_status_push(base_url):
    status = Status(base_url)
    try:
        msg = status.latest()
        assert "z" in msg and "x" in msg and "rpm" in msg
    finally:
        status.close()


def test_ws_command(base_url):
    with connect(base_url.replace("http://", "ws://") + "/ws") as ws:
        ws.send(json.dumps({"type": "cmd", "line": "display status"}))
        for _ in range(50):
            msg = json.loads(ws.recv(timeout=5))
            if msg.get("type") == "display":
                assert msg["msg"].startswith("connected=")
                return
    raise AssertionError("no display status reply")


def test_gcode_list(base_url):
    r = requests.get(base_url + "/gcode", timeout=10)
    assert r.status_code == 200
    r.json()
