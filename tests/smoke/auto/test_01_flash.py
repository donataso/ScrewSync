from conftest import FIRMWARE, TARGET, git_version


def test_flash(dut):
    assert FIRMWARE.exists(), f"{FIRMWARE} missing, build {TARGET} first"
    dut.make(f"{TARGET}-flash")
    info = dut.info()
    assert info["version"] == git_version()
    assert info["target"] == TARGET
    dut.wait_confirmed()
