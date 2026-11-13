from conftest import kv


def test_connected(dut):
    dut.ip()
    status = kv(dut.ok("wifi status"))
    assert status["ssid"], status


def test_scan(dut):
    assert dut.cmd("wifi scan") == "ok scanning"
    found = int(dut.expect(r"wifi scan done n=(\d+)", timeout=30).group(1))
    assert found >= 1
