import time

import pytest

from conftest import Status, kv

JOG_STEP_DU = 50000
MIN_ANGLE_DEG = 20


@pytest.fixture(scope="module")
def status(dut):
    s = Status(f"http://{dut.ip()}")
    yield s
    s.close()


def ask(prompt):
    try:
        return input(prompt).strip().lower()
    except (KeyboardInterrupt, EOFError):
        print()
        pytest.exit("stopped by operator")


def step(text):
    print(f"\n>>> {text}")
    answer = ask("    [Enter] run, s skip, q quit: ")
    if answer == "s":
        pytest.skip("skipped by operator")
    if answer == "q":
        pytest.exit("stopped by operator")


def done(text):
    ask(f">>> {text}, then press Enter ")


def confirm(text):
    assert ask(f">>> {text} [Y/n] ") in ("", "y"), text


def stopped_pos(status, axis, timeout=30):
    deadline = time.monotonic() + timeout
    s = status.fresh()
    while s[axis]["moving"]:
        assert time.monotonic() < deadline, f"{axis} still moving"
        s = status.fresh()
    return s[axis]["pos"]


def angle_delta(a, b):
    return (b - a + 180) % 360 - 180


def test_spindle_encoder(status):
    step("Spindle encoder: turn the spindle by hand, about a quarter turn either way")
    before = status.fresh()["angleDeg"]
    done("Turn it now")
    delta = angle_delta(before, status.fresh()["angleDeg"])
    assert abs(delta) >= MIN_ANGLE_DEG, f"angle moved {delta:.1f} deg"
    confirm(f"Angle moved {delta:+.1f} deg. You turned it {'forward' if delta > 0 else 'in reverse'}?")


@pytest.mark.parametrize("axis", ["z", "x"])
def test_handwheel_jog(status, axis):
    step(f"{axis.upper()} handwheel: turn it a few clicks, the {axis.upper()} axis will move")
    before = status.fresh()[axis]["pos"]
    done("Turn it now")
    moved = status.fresh()[axis]["pos"] - before
    assert moved != 0, "axis did not move"
    confirm(f"{axis.upper()} moved {moved:+d} steps. Correct direction")


@pytest.mark.parametrize("axis", ["z", "x"])
def test_jog_step(dut, status, axis):
    step(f"{axis.upper()} jog: moves {axis.upper()} by {JOG_STEP_DU / 10000:g} mm and back")
    dut.ok(f"jog step {JOG_STEP_DU}")
    before = status.fresh()[axis]["pos"]
    dut.ok(f"jog {axis} +")
    moved = stopped_pos(status, axis) - before
    dut.ok(f"jog {axis} -")
    assert moved > 0, f"jog + moved {moved} steps"
    assert stopped_pos(status, axis) == before, "did not come back"


def test_probe(dut, status):
    step("Probe: touch the probe to the workpiece and hold it")
    done("Hold it now")
    assert status.fresh()["probeContact"] is True
    done("Release it")
    assert status.fresh()["probeContact"] is False


def test_screen_button(dut, status):
    step(f"Screen: tap Z jog once on the display, Z moves by {JOG_STEP_DU / 10000:g} mm")
    dut.ok(f"jog step {JOG_STEP_DU}")
    before = status.fresh()["z"]["pos"]
    done("Tap it now")
    assert status.fresh()["z"]["pos"] != before, "Z did not move"


def test_display_still_connected(dut):
    assert kv(dut.ok("display status"))["connected"] == "1"
