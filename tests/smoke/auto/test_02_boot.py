from conftest import kv

BAD_RESETS = {"watchdog", "brownout", "lockup"}


def test_reset_cause(dut):
    causes = set(dut.info()["reset"].split(","))
    assert not causes & BAD_RESETS, causes


def test_not_in_estop(dut):
    assert kv(dut.ok("estop"))["state"] != "estop"
