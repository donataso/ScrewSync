import zlib

import pytest
import requests

from conftest import TFT, kv

FLASH_TIMEOUT_S = 600

pytestmark = pytest.mark.skipif(TFT is None, reason="no stored display image on this target")


def stored(dut):
    return kv(dut.ok("display info"))


def expect_stored(dut, tft):
    image = tft.read_bytes()
    info = stored(dut)
    assert info["stored"] == "1"
    assert int(info["length"]) == len(image)
    assert int(info["crc"], 16) == zlib.crc32(image)


def wait_flashed(dut):
    dut.wait_display(connected=False, timeout=30)
    dut.wait_display(connected=True, timeout=FLASH_TIMEOUT_S)


def ota(dut, base_url, tft, store):
    r = requests.post(base_url + "/update-display", data=tft.read_bytes(), headers={"X-Display-Store": store},
                      timeout=FLASH_TIMEOUT_S)
    assert r.status_code == 200, r.text
    dut.wait_display(connected=True, timeout=30)


def test_connected(dut):
    dut.wait_display(connected=True, timeout=30)


def test_flashed_image_stored(dut):
    expect_stored(dut, TFT)


def test_flash_stored(dut):
    assert dut.cmd("display flash") == "ok display flash started"
    wait_flashed(dut)


def test_serial_upload(dut):
    image = TFT.read_bytes()
    assert dut.cmd(f"display upload {len(image)} {zlib.crc32(image):08x}") == "ok upload skipped, already stored"
    expect_stored(dut, TFT)


def test_ota_not_stored(dut, base_url):
    before = stored(dut)
    ota(dut, base_url, TFT, "0")
    assert stored(dut) == before


def test_ota_stored(dut, base_url):
    ota(dut, base_url, TFT, "1")
    expect_stored(dut, TFT)
