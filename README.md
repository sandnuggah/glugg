# Glugg

*Glugg* is Swedish for a small opening in a wall. This one keeps an eye on the windows of a wood workshop.

An ESP32-S3 pairs with IKEA MYGGBETT door/window sensors and lights one LED per open window on an 8-LED stick. An ESP32-H2 is its Thread radio. It runs fully offline: no Wi-Fi, no app, no cloud.

The firmware is written and builds, but hasn't run on the real hardware yet.

## Parts

All from [Electrokit](https://www.electrokit.com/en/quickorder) except the sensors. The same list is in [`bom.csv`](bom.csv) for their quick order form.

| Part | Article no. | Qty |
|---|---|--:|
| Waveshare ESP32-S3-Zero-N8R8 | 41036435 | 1 |
| Waveshare ESP32-H2-Zero | 41032818 | 1 |
| Adafruit NeoPixel Stick, 8 LEDs | 41012479 | 1 |
| 74HCT125N quad buffer | 40380125 | 1 |
| 1N5817 Schottky diode | 40315817 | 2 |
| Resistor 330 Ω | 40811233 | 1 |
| Resistor 1 kΩ | 40811310 | 1 |
| Resistor 10 kΩ | 40811410 | 1 |
| Capacitor 1000 µF 16 V | 41017694 | 1 |
| Capacitor 100 nF | 41015538 | 1 |
| IKEA MYGGBETT door/window sensor | from IKEA | up to 8 |

You'll also need a USB-C charger and a plain USB-C breakout board for power (the kind with 5.1 kΩ resistors on its CC pins, not a PD trigger board), pin headers (neither board comes with them), something to build on, and wire.

## Wiring

![Wiring diagram](docs/wiring.svg)

- The S3's 5V pin is fed from the +5 V rail through a 1N5817, and the H2's 5V pin from the S3's 5V pin through the second 1N5817, so the H2 is powered whenever the S3 is.
- The S3's GPIO13 drives the LEDs through the 74HCT125 and the 330 Ω resistor. Tie the 74HCT125's unused inputs (pins 4, 5, 9, 10, 12, 13) to GND.
- The two boards talk over two crossed UART wires, with the 1 kΩ resistor at the S3 end.
- Power comes from a USB-C charger through the USB-C breakout: its VBUS is the +5 V rail. Measure 5 V on it before connecting anything.

## Usage

Plug in the power. The LEDs sweep white for 10–20 seconds while the network starts, then show each window:

| LED | Means |
|---|---|
| Amber | Window open |
| Off | Window closed, or no sensor in this slot |
| Green pulse | Pairing a sensor into this slot |
| Dim white | Sensor paired but hasn't reported yet |
| Blue blink | Sensor not heard from for 2 minutes (battery, range) |

LED 1 is the end nearest the wires. To manage sensors, connect a computer to the S3's USB-C port and open its serial console (for example `idf.py monitor`):

```
pair 3497-011-2332       add a sensor
list                     show all 8 slots
remove 3                 remove the sensor in slot 3
identify 3               make sensor 3 blink its own light
factory-reset confirm    remove every sensor and start over
```

Connect through a USB hub or an adapter that stays plugged into the computer. Plugging a USB-C cable straight in while the power is on may not work.

## Pairing

- Use the 11-digit Matter code printed on the sensor, not the yellow IKEA app code.
- Open the sensor's pairing window first: insert the battery or press its button once. It stays open for 15 minutes.
- Type `pair` and the code. The new sensor takes the lowest free slot, so the first sensor gets LED 1.
- A sensor that's already paired somewhere else won't pair again until you factory reset it: hold the button for 10 seconds.
- If `remove` can't reach a sleeping sensor, factory reset it the same way before pairing it again.
