# ESP32 USB display firmware

USB display firmware for **Waveshare ESP32-C6 1.47-inch** and **ESP32-WROOM-32 1.96-inch ST7789** hardware. A compatible player-side bridge supplies time, metadata, artwork and display control over USB. No Wi-Fi setup is needed on the ESP32.

The source comes from the PogoPlug V6 display project. The display reports **P5 firmware 5.3**; player image and display firmware versions are separate.

## Supported hardware

| Display | PlatformIO environment | Landscape resolution | USB interface |
| :--- | :--- | :--- | :--- |
| Waveshare ESP32-C6 1.47-inch | `waveshare_c6_147` (default) | 320 × 172 | Native USB CDC |
| ESP32-WROOM-32 1.96-inch ST7789 | `st7789` | 320 × 170 | Board USB-to-serial interface |

The WROOM profile assumes the wiring below; it is not a generic profile for every 1.96-inch display. ILI9341 is not a supported build profile in this directory.

## Features

- Host-supplied time, date and UTC offset.
- Scrolling titles, artists and radio station information.
- Host-supplied JPEG cover artwork up to 80 × 80 pixels.
- Flip clock for host modes `off`, `stop` and `pause`.
- Host-controlled backlight brightness and display enable.
- A connection indicator for stale host data.

This source changes to the flip clock immediately when the host reports an inactive mode. It is separate from the experimental firmware with a 10-second standby delay. Metadata and artwork availability depend on the player-side bridge and playback service.

## Requirements

- Git, Python 3 and PlatformIO Core, or VS Code with PlatformIO IDE.
- Internet access for the first build to download toolchains and dependencies.
- A USB data cable and one of the supported boards.
- A player image with the P5-compatible `pogo-display` bridge and the required USB serial support.

## 1. Open the project

```sh
git clone https://github.com/arm-audiobox/arm-audiobox.git
cd arm-audiobox/esp32-display-firmware
```

If already cloned, open the existing `esp32-display-firmware` directory. Run all following commands from that directory.

If `pio` is not installed, create a Python virtual environment **outside the checkout**. For example, on macOS or Linux:

```sh
python3 -m venv "$HOME/.venvs/arm-audiobox-pio"
. "$HOME/.venvs/arm-audiobox-pio/bin/activate"
python -m pip install platformio
pio --version
```

On Windows, use PlatformIO IDE or activate a Python virtual environment with its `Scripts/Activate.ps1` script. In VS Code, open this firmware directory as the project, rather than the repository root.

## 2. Build for the selected display

### Waveshare ESP32-C6 1.47-inch

On macOS or Linux:

```sh
./build-waveshare.sh
```

The helper installs the pinned Arduino 3 framework before the build. Equivalent commands, also suitable for a Windows terminal:

```sh
pio pkg install -e waveshare_c6_147
pio run -e waveshare_c6_147
```

The profile uses pioarduino platform `55.03.37`, Arduino ESP32 `3.3.7`, an ESP32-C6 target and an 8 MB partition layout. Keep the native USB flags, JPEG symbol-renaming flags and `build-support.py` hook in place.

### ESP32-WROOM-32 1.96-inch ST7789

```sh
pio run -e st7789
```

This profile uses Espressif32 platform `6.9.0` with the classic `esp32dev` target. Build the two profiles sequentially: they use different Arduino framework versions. After a WROOM build, run the C6 helper again before building or uploading for C6.

## 3. Flash the firmware

Find the port:

```sh
pio device list
```

Typical names are `/dev/cu.usbmodem...` for C6 on macOS, `/dev/cu.usbserial...` for a USB-to-serial board, `/dev/ttyACM0` or `/dev/ttyUSB0` on Linux, and `COM5` on Windows. Replace `YOUR_PORT` below with your actual port.

**Waveshare C6:**

```sh
pio pkg install -e waveshare_c6_147
pio run -e waveshare_c6_147 -t upload --upload-port YOUR_PORT
```

**WROOM-32:**

```sh
pio run -e st7789 -t upload --upload-port YOUR_PORT
```

Use PlatformIO's upload target for initial installation: it selects the bootloader, partition table and application offsets for the chosen board. Do not flash a C6 build to WROOM hardware or vice versa. No prebuilt binaries are included here.

If uploading cannot connect, hold BOOT, press and release RESET, then release BOOT and retry. If there is no RESET button, reconnect USB while holding BOOT. The C6 port can change between download mode and application mode; run `pio device list` again. Reset after upload if the board stays in download mode.

## 4. Verify startup

```sh
pio device monitor --port YOUR_PORT --baud 115200
```

Press RESET while monitoring. Expected startup output for C6:

```text
P5 READY 5.3 320x172 Waveshare ESP32-C6 1.47
```

For WROOM:

```text
P5 READY 5.3 320x170 ESP32-WROOM-32 1.96″
```

Close the serial monitor before another program uses the port. Without host frames, the display initially shows a USB connection prompt. It cannot obtain time or track data by itself.

## 5. Connect to the player

1. Disconnect the display from the build computer and connect it to the player's USB host port with a data cable.
2. Use a player image containing the P5-compatible `pogo-display` bridge and support for the board's USB serial interface.
3. Enable the display in the player's WebIF and select the matching model or device where available.
4. Configure the music server on the player and start playback. Check the time, title, artist and cover.
5. Pause or stop playback to check the flip clock; then test brightness and display enable.

Model detection and WebIF settings are provided by the player image. This directory contains ESP32 firmware only; it does not install the host bridge or add display support to arbitrary PogoPlug or audioBox images.

## Pin assignments

See [DisplayConfig.h](include/DisplayConfig.h) for the build configuration.

| Signal | Waveshare C6 1.47-inch | WROOM-32 1.96-inch |
| :--- | ---: | ---: |
| SPI clock | GPIO 7 | GPIO 18 |
| SPI MOSI | GPIO 6 | GPIO 23 |
| SPI MISO / shared bus | GPIO 5 | GPIO 19 |
| TFT CS | GPIO 14 | GPIO 15 |
| TFT DC | GPIO 15 | GPIO 2 |
| TFT reset | GPIO 21 | GPIO 4 |
| Backlight | GPIO 22 | GPIO 32 |
| SPI frequency | 12 MHz | 20 MHz |

The Waveshare display is integrated; the table describes its existing wiring. Its microSD chip-select GPIO 4 is held HIGH by the firmware. For external WROOM wiring, check the exact board and panel, use a common ground and observe the module's supply requirements. TFT MISO is not required for the write-only display interface.

Both profiles use active-HIGH backlight control, landscape rotation `1` and ST7789 inversion. On C6, 100% requested brightness maps to a maximum physical PWM duty of **127/255**. Keep that limit in the configuration.

## Troubleshooting

| Symptom | Check |
| :--- | :--- |
| No USB port | Data cable, USB connection and the driver for the board's actual interface. |
| Upload does not connect | Correct environment and port, monitor closed, BOOT/RESET sequence. |
| C6 reports missing `FS.h` | Use `build-waveshare.sh`; keep the pre-build hook and pinned packages. |
| Blank or incorrect image | Board model, ST7789 panel dimensions, pin mapping, rotation and backlight polarity. |
| USB prompt / no clock | The host bridge must open the correct device and send valid time and metadata frames. |
| No cover | The host must send a CRC-checked JPEG up to 80 × 80 pixels; metadata alone does not contain the image. |
| No brightness response | The host must send P5D commands and use the configured backlight GPIO. |

## Protocol and source checks

Serial speed is **115200 baud**. The host sends newline-terminated, tab-separated `P5` metadata/time frames, `P5C` cover frames and `P5D` control frames. Metadata text fields are percent encoded. See [DisplayProtocol.h](include/DisplayProtocol.h) and [main.cpp](src/main.cpp).

On macOS or Linux, run the imported parser and scrolling tests with a C++ compiler:

```sh
c++ -std=c++11 -I include test-protocol.cpp -o /tmp/esp32-display-protocol-test
/tmp/esp32-display-protocol-test
```

## Dependencies and provenance

Adafruit ST7735/ST7789, GFX and BusIO libraries are vendored under `lib/`, with their existing copyright and license notices. PlatformIO downloads `TJpg_Decoder` pinned to `1.1.0`. Retain third-party notices when redistributing the source.

See [SOURCE.md](SOURCE.md) for the source origin and import changes. Generated builds, serial logs, device identifiers and prebuilt firmware are excluded from this import.

[Back to arm-audiobox](../README.md)
