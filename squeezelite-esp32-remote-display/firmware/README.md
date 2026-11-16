# Firmware images

Current version: **0.13.1**, internal build ID **0.13.1-overlay-test**. The update binary is byte-for-byte identical to the application flashed and approved on the development CYD.

| File | Flash offset | Purpose |
| :--- | :--- | :--- |
| `cyd-st7789-v0.13.1-factory.bin` | `0x0` | First installation; includes bootloader and partitions and overwrites the settings area. |
| `cyd-st7789-v0.13.1-update.bin` | `0x10000` | Application update with the same partition layout; keeps device settings. |
| `bootloader.bin` | `0x1000` | Current bootloader. |
| `partitions.bin` | `0x8000` | Current 4 MB flash partition table. |
| `boot_app0.bin` | `0xe000` | Boot-app initialization image. |

The v0.11 application and factory binaries remain available as earlier versions. `manifest.json` describes the current version. `SHA256SUMS` covers all binary files in this directory.

No binary is a dump of device flash. Stored Wi-Fi credentials and calibration are not included. See the project README for flashing commands.
