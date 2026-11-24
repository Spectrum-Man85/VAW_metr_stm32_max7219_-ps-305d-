# Settings menu

**English** · [Русский](../settings.md)

[← Project overview](../../README.md) · [Calibration](calibration.md) · [Hardware](hardware.md)

## Opening and using the menu

Power on **without holding the button**. In normal measurement mode, hold the button for more than **0.8 seconds**, then release it. The menu opens after approximately 0.4 seconds. Alternatively, use a double short press.

| Action | Result |
|---|---|
| Turn while not editing | Select item 1–19 |
| Short press | Start editing |
| Turn while editing | Change the value |
| Another short press | Finish editing |
| Hold for more than 0.8 s, then release | Exit the menu and save changes |

**Left field: item number. Right field: value.** The right-hand field blinks while editing. One encoder detent corresponds to one menu step.

## Settings reference

| No. | Parameter in the right field | Values and increment |
|---:|---|---|
| 1 | Global current coefficient `kaa` | Increment 0.0001; direct coefficient adjustment |
| 2 | INA226 bus-voltage conversion time, µs | 140 / 204 / 332 / 588 / 1100 / 2116 / 4156 / 8244 |
| 3 | INA226 shunt-current conversion time, µs | Same options |
| 4 | INA226 averaging in normal mode | 1 / 4 / 16 / 64 / 128 / 256 / 512 / 1024 |
| 5 | INA226 averaging during calibration | Same options |
| 6 | Heatsink fan start temperature, °C | 30–100, increment 1 |
| 7 | Heatsink temperature for maximum PWM, °C | 30–100, increment 1 |
| 8 | Minimum heatsink fan PWM | 0–255, increment 1 |
| 9 | Maximum heatsink fan PWM | Minimum PWM to 255, increment 1 |
| 10 | Transformer fan start temperature, °C | 1–100, increment 1 |
| 11 | Transformer temperature for maximum PWM, °C | Start temperature to 100, increment 1 |
| 12 | Minimum transformer fan PWM | 0–255, increment 1 |
| 13 | Maximum transformer fan PWM | Minimum PWM to 255, increment 1 |
| 14 | Critical heatsink temperature, °C | 0–100, increment 1; stored, but emergency shutdown is not implemented |
| 15 | Transformer winding switching hysteresis, V | 0–10, increment 0.1 |
| 16 | Audible feedback | 1 — off; 2 — startup and saving; 3 — all feedback |
| 17 | Buzzer type | 0 — passive; 1 — active |
| 18 | Shunt voltage-drop compensation | 0 — off; 1 — on |
| 19 | Shunt resistance for compensation, Ω | 0.001–0.500, increment 0.001 |

The table is checked against [memory_view.cpp](../../platformio/src/memory_view.cpp). Values saved on a particular device may differ from the source-code defaults.

## Fan configuration example

For the heatsink fan:

| Item | Value | Effect |
|---:|---:|---|
| 6 | 35 | Fan off below 35 °C |
| 7 | 50 | Maximum PWM reached at 50 °C |
| 8 | 60 | PWM 60 at the start of the operating interval |
| 9 | 255 | Maximum PWM is 100% |

Between 35 and 50 °C, the target PWM rises linearly from 60 to 255. Items 10–13 apply the same principle to the transformer fan. Set the upper temperature above the start temperature.

## Display and saving details

- PWM 255 is 100%; 128 is approximately 50%.
- Item 1's 0.0001 increment is finer than the four-digit display can show in some cases, so each encoder step may not produce a visible change.
- Items 2–3 configure normal measurements. During calibration, the firmware uses 1100 µs per channel and the averaging value from item 5.
- Changes take effect while editing. A long press to exit saves them; changes can also be saved after 20 seconds without turning the encoder.
- Display brightness, winding switching thresholds, the driver's shunt configuration, and PWM frequency are changed in the source code rather than through this menu.
