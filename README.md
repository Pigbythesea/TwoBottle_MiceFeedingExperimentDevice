# TwoBottle Feeding Device

This is a two-bottle, home-cage mice operant training device that records mice behavior for experimentation.

---

## Table of Contents

1. [Introduction](#introduction)
2. [Key Features](#key-features)
3. [Hardware Overview](#hardware-overview)
4. [Bill of Materials](#bill-of-materials)
5. [Pin‑Out Reference](#pin-out-reference)
6. [Software Requirements](#software-requirements)
7. [Installation & Flashing](#installation--flashing)
8. [Runtime Modes](#runtime-modes)
9. [Data Logging](#data-logging)
10. [Calibrating Dispense Volume](#calibrating-dispense-volume)
11. [Extending the Library](#extending-the-library)
12. [License](#license)
13. [Acknowledgements](#acknowledgements)

---

## Introduction

**TwoBottle** is an open‑source library and firmware bundle that converts a Teensy 4.1 into a dual‑bottle liquid reward rig.  It inherits the proven architecture of the **FED3** pellet feeder (Kravitz et al., 2019‑2021) while substituting stepper‑driven syringe pumps for rapid 10 µL deliveries, and adding capacitive lick detection on each side. The result is a compact, self‑contained apparatus for operant or Pavlovian paradigms in rodents.

All hardware control, task logic, and SD logging are encapsulated in a single C++ class, defined in **TwoBottle.h / TwoBottle.cpp**.  Example sketches simply instantiate the class, select or custom edit a sketch, and call `run()` inside `loop()`—leaving you free to focus on experimental design rather than boilerplate.

## Key Features

* **Dual independent liquid lines** — left & right liquid delivery driven by 2 discrete stepper 4‑wire unipolar motors with TB6612 Control Board.
* **High‑speed dispense** — configurable `dispenseRPM` and per‑side step count (`doseLeftSteps`, `doseRightSteps`) yield \~10 µL in < 250 ms.
* **Capacitive lick sensing** — Adafruit MPR121 breakout with IRQ handling down to 5 ms resolution.
* **Rich behavioural schedules** — Lick to pump, FR/PR, customizable sketch for behavioral paradigms with BNC output
* **SD/CSV logging** — millisecond‑stamped events with battery, poke/lick counts, dispense counts.
* **TTL/BNC output** — optically isolate external stimulators with precise pulse width & frequency control (`pulseGenerator()`).

## Hardware Overview

```
 ┌────────────────────────────────────────────────────────────┐
 │ Teensy 4.1                                                 │
 │  ├── I²C0  SDA0/SCL0  → AHT20  (temp & RH)                 │
 │  ├── I²C2  SDA2/SCL2  → MPR121 (lick sensor)  IRQ → Pin 9  │
 │  ├── SPI   SCK/MOSI/SS → Sharp Memory LCD                  │
 │  ├── GPIO 22/21       → Infra‑red nose‑pokes (L/R)         │ 
 │  ├── GPIO 16‑17‑14‑13 → Stepper LEFT  (L_IN1–4)            │
 │  ├── GPIO 36‑37‑34‑33 → Stepper RIGHT (R_IN1–4)            │
 │  ├── GPIO 15 / 35     → Motor‑driver enable (L/R)          │
 │  ├── GPIO 30          → Green status LED                   │
 │  ├── GPIO  3          → Piezo buzzer                       │
 │  └── GPIO 23          → BNC/TTL out                        │
 └────────────────────────────────────────────────────────────┘
```
See the complete schema for pin mapping in twobottle.h file. 

## Bill of Materials

| Qty | Component                                 |
| :-: | ----------------------------------------- |
|  1  | Teensy 4.1 (with pins)                    |
|  2  | 5 V stepper + TB6612 driver               |
|  1  | Adafruit MPR121 capacitive touch breakout |
|  1  | Adafruit AHT20 sensor (optional)          |
|  1  | Sharp LS013B7DH06 memory LCD 1.3 "        |
|  2  | Infra‑red beam‑break sensor pairs         |
|  1  | microSD card (≥ 1 GB, class 10)           |
|  •  | 3D‑printed enclosure & bottle mounts      |


## Installation & Flashing

```bash
# 1. Clone the repo
$ git clone https://github.com/Pigbythesea/TwoBottle_FeedingDevice.git

# 2. Open the example sketch
#    Documents → Arduino → Libraries → TwoBottle_FeedingDevice → examples

# 3. Tools → Board: "Teensy 4.1"  |  Tools → USB Type: "Serial"
# 4. Sketch → Upload (or `arduino-cli compile --upload`)
```

After flashing, the LCD splash will appear. Brief taps on the left or right poke decrement/increment the value, respectively.

## Data Logging

Each event appends a line to `FED###_MMDDYYNN.CSV` on the microSD.  Columns include time‑stamp (to ms), battery voltage, left/right motor turns, lick & poke counts.

To parse the CSV in Python:

```python
import pandas as pd

log = pd.read_csv('FED001_08072500.CSV', parse_dates=['MM:DD:YYYY hh:mm:ss:ms'])
log['elapsed_s'] = (log['MM:DD:YYYY hh:mm:ss:ms'] - log['MM:DD:YYYY hh:mm:ss:ms'][0]).dt.total_seconds()
```

## Calibrating Dispense Volume

1. Fill syringes with water and prime the lines.
2. Place a tared micro‑balance under the spout.
3. Set `doseLeftSteps` (or right) to an initial guess and upload.
4. Trigger 10 spins via serial `FeedLeft(steps)`; weigh the dispensed mass.
5. Adjust the step count proportionally: `new = old × (target_mass / measured_mass)`.
6. Repeat until a single dispense weighs 10 ± 0.5 mg.

## Extending the Library

* Add new behavioural schedules by subclassing **FED3** or writing wrapper sketches.
* `pulseGenerator()` supports closed‑loop optogenetics—modify it for patterned trains.
* The motor interface accepts any `Stepper`‑compatible driver; simply re‑map `L_IN*` / `R_IN*` pins.

## License

**Creative Commons Attribution‑ShareAlike 4.0 International**
© 2025 Zihuan Zhang — forked from **FED3** (GPL‑3.0).  You are free to use, remix, and distribute, provided you give appropriate credit and distribute derivatives under the same license.

## Acknowledgements

This project stands on the shoulders of open‑source giants:

* **Lex Kravitz et al., Kravitz Lab** — original FED3 concept and codebase.
* **Adafruit Industries** — breakout boards and driver libraries.
* **PJRC** — the formidable Teensy platform.

*If you use TwoBottle in published work, please cite the FED3 paper (Krutz et al., 2021) and credit this repository.*
