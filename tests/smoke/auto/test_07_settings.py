import re

FIELD = "statusPushIntervalMs"


def read(dut):
    return int(re.search('"' + FIELD + r'":(\d+)', dut.ok("settings get")).group(1))


def test_persists_over_reboot(dut):
    value = 750 if read(dut) != 750 else 800
    dut.ok(f"settings set {FIELD} {value}")
    dut.send("reboot")
    # Closing the port before the device reads the line can lose it.
    dut.expect(r"^ESP-ROM:", timeout=10)
    dut.wait_rebooted()
    assert read(dut) == value
