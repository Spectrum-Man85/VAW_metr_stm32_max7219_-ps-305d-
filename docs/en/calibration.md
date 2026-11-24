# Measurement calibration

**English** · [Русский](../calibration.md)

[← Project overview](../../README.md) · [Hardware](hardware.md) · [Settings](settings.md)

## Two levels of correction

Each measurement channel has **one global coefficient** and **100 local coefficients**. The global coefficient sets the overall scale; local coefficients correct deviations in individual parts of the measurement range. Values are interpolated between the centers of neighboring ranges.

| Channel | Local coefficients | Grid interval |
|---|---:|---:|
| Voltage | 100 | 0.36 V |
| Current | 100 | 0.1 A |

Adjust the global coefficients first, followed by the local coefficients. Use a stable source, a suitable load, and a reference measurement instrument.

## Global calibration: GU → GA → measurements

| Stage | Left field | Right field | A new short press |
|---|---|---|---|
| Global voltage | GU | Measured voltage | Save voltage and move to GA |
| Global current | Measured current | GA | Save current and return to measurements |

1. Connect a reference voltmeter and prepare a stable, nonzero voltage, for example around 10 V.
2. Hold the encoder button while powering on. This resets the global and local calibration of **both channels** and opens the GU stage.
3. Release the button. The display and measurements keep updating; the startup hold and release do not count as a click. New presses are accepted after the released state has remained stable for 50 ms.
4. Turn the encoder until the voltage on the right matches the reference instrument.
5. Make a new short press. The global voltage coefficient is saved, and the device moves to GA **without restarting**.
6. Connect a stable load and a reference ammeter. Adjust the current on the left to match the reference. Calibration uses **actual INA226 current readings**, not a synthetic test value.
7. Make another short press to save the global current coefficient and return to normal measurements.

Initial coefficients are saved when confirmed with the button; there is no need to wait 30 seconds after confirmation. If saving fails, the device remains at the current stage. The regular settings menu and automatic calibration timeout are disabled during initial calibration.

Make separate presses with about a one-second pause: a double press from normal measurement mode opens the settings menu.

## Local calibration

Once global calibration is complete, power on without holding the button.

1. Set the desired voltage or current.
2. Make one short press to enter voltage calibration; make another to enter current calibration.
3. Turn the encoder until the reading matches the reference instrument. This changes the local coefficient for the current range.
4. Repeat at other points. The opposite display field shows the range number, from 1 to 100.
5. After the last adjustment, wait **more than 30 seconds without encoder activity** before switching off.

Regular local calibration returns to measurement mode after 5 seconds without turning the encoder. This **does not mean** that the 30-second save delay has elapsed.

## Stored data

| Region | Contents |
|---|---|
| Bytes 0–839 | Voltage and current calibration, base-calibration completion flags, and checksums |
| Bytes 848–935 | Device settings: INA226, cooling, sound, and shunt compensation |

Storage uses STM32 EEPROM emulation. Local coefficients from the old nonuniform MAX41 grid are not reused when migrating to the new grid; the device must be recalibrated.

## Important distinctions

- The global current coefficient can also be changed directly in menu item 1. That item edits the coefficient itself rather than matching a displayed measurement to a reference.
- Menu item 19 changes the resistance used for voltage compensation. The measurement driver's shunt value is configured separately in the source code.
- Powering on with the button held resets calibration. To enter the regular settings menu, power on without pressing the button and use a long press after startup.
