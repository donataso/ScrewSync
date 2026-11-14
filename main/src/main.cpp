#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "Console.hpp"
#include "PersistBlob.hpp"
#include "Settings.hpp"
#include "Pins.hpp"
#include "Encoder.hpp"
#include "Handwheel.hpp"
#include "Stepper.hpp"
#include "RunSettings.hpp"
#include "Machine.hpp"
#include "Watchdog.hpp"
#include "Mode.hpp"
#include "ProbeInput.hpp"
#include "ThreadStore.hpp"
#include "Jog.hpp"
#include "Rpm.hpp"
#include "Wifi.hpp"
#include "WebUi.hpp"
#include "Display.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF); // Wifi declares against this module

int main(void)
{
    PersistBlob::preload();
    Machine::begin();
    Settings::load();
    Pins::begin();
    Encoder::begin();
    Handwheel::begin();
    Stepper::begin();
    ProbeInput::begin(Mode::probeEdge);
    Watchdog::begin();
    RunSettings::load();
    ThreadStore::begin();
    Mode::begin();
    Jog::begin();
    Rpm::begin();
    Display::begin();
    Wifi::begin();
    WebUi::begin();

    while (1) {
        consolePoll();
        Wifi::loop();
        WebUi::poll();
        k_sleep(K_MSEC(50));
    }
}
