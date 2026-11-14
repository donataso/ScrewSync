import pytest
import requests

from conftest import FIRMWARE, FIXTURES, git_version

SWAP_TIMEOUT_S = 180


def upload(base_url, path):
    try:
        return requests.post(base_url + "/update", data=path.read_bytes(), timeout=180).status_code
    except requests.ConnectionError:
        return None


def test_ota(dut, base_url):
    dut.wait_confirmed()
    assert upload(base_url, FIRMWARE) == 200
    info = dut.wait_rebooted(SWAP_TIMEOUT_S)
    assert info["version"] == git_version()
    assert info["confirmed"] == "0"
    dut.wait_confirmed()


@pytest.mark.parametrize("image", ["garbage.bin", "bad_signature.bin"])
def test_ota_rejected(dut, base_url, image):
    dut.wait_confirmed()
    if upload(base_url, FIXTURES / image) == 200:
        dut.wait_rebooted(SWAP_TIMEOUT_S)
    info = dut.info()
    assert info["version"] == git_version()
    assert info["confirmed"] == "1"
