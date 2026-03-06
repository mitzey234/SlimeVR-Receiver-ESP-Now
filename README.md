# SlimeVR HID ESPNow Dongle

This is a project that implements an alternative communication method for
SlimeVR trackers using the [ESPNow Protocol](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) 
available on ESP devices.

## Building and Flashing

To firmware currently only supports ESP32-S2 & ESP32-S3 based dongles. To get a board working, add the necessary JSON file 
in the `boards/` directory and create a new directory and `pins_arduino.h` file
under `variants/`. After that, adding a new `env` definition in the
platformio.ini file should work.

To flash the dongle, run the `pio run -t upload` command.

## Usage

To use the dongle, it needs to be connected to a PC through USB. You also need
your trackers to have [compatible firmware](https://github.com/mitzey234/SlimeVR-Tracker-ESP/tree/esp-now)
flashed onto them.

You can also control this dongle through a serial console, which is useful for debugging and for pairing trackers without the need to press the button on the dongle. I recommend using [SlimeVR ESP Dongle Manager](https://github.com/mitzey234/SlimeVR-ESP-Dongle-Manager) for this, as it provides a nice GUI for managing the dongle and its paired trackers.

The trackers will require pairing the first time you set them up. To achieve
this, first you need to put the dongle into pairing mode by pressing the button
on it once while the trackers are in pairing mode. Pairing mode is indicated by a rapidly flashing light on both the dongle and the tracker. If the pairing was successful, the tracker will stop flashing rapdidly and switch to a slower infrequent flashing pattern. The dongle only exits pairing mode after 60 seconds of inactivity, or if you press the button on it again. Pairing inactivity is reset when a new tracker is paired.

After pairing, when you turn the tracker on from that point on, it will attempt to
connect to its saved dongle.

If everything went as expected, the tracker should now appear in the SlimeVR server.

If the dongle LED flashes with longer flashes after being plugged in, it means it is scanning the wireless environment for the best channel to use. This process is necessary for optimal performance, and it only happens the first time the dongle is powered on. Once the scanning is complete, the dongle will save the best channel found and use it for future connections. The LED will stop flashing rapidly once the scanning is complete. You can also exit the scanning mode manually by pressing and holding the button on the dongle for 5 seconds (it will not unpair your trackers).

To rescan the environment, press the button on the dongle twice quickly. Note that you cannot enter pairing mode while the dongle is scanning the environment.

If you ever want to pair the tracker to a different dongle your best course of action is to put it into pairing mode with UART serial commands, or by unpairing all trackers WHILE they are connected to the dongle.

If you press and hold the pair button on the dongle for more than 5 seconds, it will unpair all connected trackers as well as erase all saved pairing information for any trackers (connected or not). It will also tell all connected trackers to unpair themselves and enter pairing mode, allowing you to pair them to a different dongle if desired.

## Errors

In case something goes wrong, theres not a lot of debugging information available apart from the serial console.

That being said don't expect me to provide very much support for this project, as I made it as a proof of concept that hopefully others can build upon to perhaps create ESP-now support for SlimeVR ESP based trackers.
