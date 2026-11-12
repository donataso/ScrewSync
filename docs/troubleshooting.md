# Troubleshooting

## "No connection to controller", no Wi-Fi

The ScrewSync screen shows this message, so the display firmware works.
The controller does not start, or does not talk to the display. A
controller that runs with no saved network always opens a `ScrewSync-xxxx`
access point.

1. Read the boot log. Install pyserial and start the monitor:

   ```bash
   pip install pyserial
   python tools/monitor.py
   ```

   The monitor reconnects automatically, so it also gets the first boot
   messages. Power on the board and connect USB. When the monitor is
   connected, type `reboot`.

2. Get the flash chip data:

   ```bash
   esptool.py --chip esp32s3 flash-id
   ```

3. Erase the settings region. Old settings can stop the controller. The
   firmware stays installed.

   ```bash
   esptool.py --chip esp32s3 erase-region 0x3b0000 0x30000
   ```

4. Disconnect all cables except USB and the display, then start again.

## Support

1. Do the troubleshooting steps first.
2. Look in the [existing issues](https://github.com/donataso/ScrewSync/issues)
   for the same problem.
3. If there is none, open a new issue. Write it in English. Include:
   - the board version
   - how you flashed the controller and the display (tool, file, address)
   - the full monitor output after `reboot`
   - the `flash-id` output
