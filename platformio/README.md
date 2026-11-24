# VAW Meter firmware

**English** · [Русский](README.ru.md)

This is the PlatformIO project directory for STM32F103C8T6, MAX7219, and INA226.

**[Project overview, photographs, and schematics →](../README.md)**

| Topic | Guide |
|---|---|
| Pinout and schematics | [Hardware](../docs/en/hardware.md) |
| Measurement adjustment | [Calibration](../docs/en/calibration.md) |
| Temperatures, fans, and other parameters | [Settings menu](../docs/en/settings.md) |

Run these commands from this directory:

```bash
pio run
pio run --target upload
```

The current programmer is J-Link; the build environment is `genericSTM32F103C8`.

The main documentation reflects the current source code. The older Russian `Readme_RU.txt` and settings text file are retained as source material.
