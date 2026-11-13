# Source provenance

Imported on 2026-11-14 from the build machine's `PogoPlug/pogoplug-oxnas-v60/display` source directory. The firmware reports P5 version 5.3. The E02 V6 and OXNAS V6 `src` directories compared equal during source selection.

## Import changes

- Retained the Waveshare ESP32-C6 1.47-inch (`waveshare_c6_147`) and ESP32-WROOM-32 1.96-inch ST7789 (`st7789`) build profiles.
- Set Waveshare C6 as the default environment.
- Omitted the unused ILI9341 library, generated builds, logs, examples and operating-system metadata.
- Added English installation documentation and project ignore rules.
- Kept firmware application code, hardware configuration, protocol tests and build-support scripts unchanged.

The Adafruit library sources retain their existing upstream notices. The repository-level license does not replace third-party licenses.

No physical device was flashed during this import.
