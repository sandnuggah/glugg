# Glugg

*Glugg* is Swedish for a small opening in a wall. This one keeps an eye on the windows of a wood workshop.

An ESP32-S3 pairs with IKEA MYGGBETT door/window sensors and lights one LED per open window on an 8×8 LED panel. An ESP32-H2 is its Thread radio. It runs fully offline: no Wi-Fi, no app, no cloud.

It runs on a hand-soldered protoboard with two MYGGBETT. The firmware supports up to 64 sensors, one per LED.

## Parts

All from [Electrokit](https://www.electrokit.com/en/quickorder) except the sensors and the LED panel. The same list is in [`bom.csv`](bom.csv) for their quick order form.

| Part | Article no. | Qty |
|---|---|--:|
| Waveshare ESP32-S3-Zero-N8R8, with headers | 41036436 | 1 |
| Waveshare ESP32-H2-Zero, with headers | 41032832 | 1 |
| 8×8 WS2812 LED panel (not from Electrokit) | | 1 |
| 74HCT32 quad OR gate, used as a buffer | 40381032 | 1 |
| DIL socket 18-pin (holds the 74HCT32) | 40410018 | 1 |
| 1N5817 Schottky diode | 40315817 | 1 |
| Resistor 330 Ω | 40811233 | 1 |
| Resistor 1 kΩ | 40811310 | 1 |
| Resistor 10 kΩ | 40811410 | 1 |
| Capacitor 1000 µF 16 V | 41017694 | 1 |
| Capacitor 100 nF | 41015538 | 1 |
| Screw terminal 2.54 mm, 3-pole | 41002454 | 1 |
| Prototype PCB 40×60 mm | 41010656 | 1 |
| IKEA MYGGBETT door/window sensor | from IKEA | up to 64 |

You'll also need a USB-C charger and cable, and solid-core wire.

## Wiring

![Wiring diagram](docs/wiring.svg)

- Power comes from a USB-C charger plugged into the S3. The S3's 5V pin is the +5 V rail: it feeds the 74HCT32 and the LED panel directly, and the H2's 5V pin through the 1N5817.
- The S3's GPIO13 drives the LEDs through the 74HCT32 (into pins 9 and 10, out of pin 8) and the 330 Ω resistor. Tie the unused inputs to +5 V (pins 1, 2, 12, 13) or GND (pins 4, 5).
- The hole-by-hole protoboard layout is in [`docs/protoboard-small.svg`](docs/protoboard-small.svg) (top) and [`docs/protoboard-small-wiring.svg`](docs/protoboard-small-wiring.svg) (underside).
- The two boards talk over two crossed UART wires, with the 1 kΩ resistor at the S3 end.

## Usage

Plug in the power. A dim white ring breathes out from the middle of the panel for 10–20 seconds while the network starts. Then each sensor's LED shows its window:

| LED | Means |
|---|---|
| Amber | Window open |
| Off | Window closed, or no sensor in this slot |
| Green pulse | Pairing a sensor into this slot |
| Dim white | Sensor paired but hasn't reported yet |
| Blue blink | Sensor not heard from for 2 minutes (battery, range) |

LED 1 is the first LED after the panel's DIN pad; LED 2 onwards follow the panel's data chain. To manage sensors, connect a computer to the S3's USB-C port and open its serial console (for example `idf.py monitor`):

```
pair 3497-011-2332            add a sensor on the lowest free LED
pair 3497-011-2332 slot 14    add a sensor on LED 14
list                          show the paired sensors
move 14 20                    put sensor 14 on LED 20 (swaps if 20 is used)
remove 3                      remove the sensor in slot 3
identify 3                    ask sensor 3 to blink its own light, if it has one
factory-reset confirm         remove every sensor and start over
```

Power and the console share the S3's USB-C port: unplug the charger and plug in the computer instead (Glugg restarts, and the computer powers it meanwhile). A USB-C hub with power pass-through lets you keep both connected.

## Pairing

- Use the 11-digit Matter code printed on the sensor, not the yellow IKEA app code.
- Open the sensor's pairing window first: insert the battery or press its button once. It stays open for 15 minutes.
- Type `pair` and the code, and optionally `slot` and an LED number (1–64). Without a slot, the new sensor takes the lowest free one, so the first sensor gets LED 1.
- A sensor that's already paired somewhere else won't pair again until you factory reset it: hold the button for 10 seconds.
- If `remove` can't reach a sleeping sensor, factory reset it the same way before pairing it again.
