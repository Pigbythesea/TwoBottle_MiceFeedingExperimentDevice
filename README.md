# TwoBottle Feeding Device

*This is a two-bottle, home-cage mice operant training device that records mice behavior for experimentation.*

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

**TwoBottle** is an open‑source library and firmware bundle that converts a Teensy 4.1 into a dual‑bottle liquid reward rig.  It inherits the proven architecture of the **FED3** pellet feeder (Kravitz et al., 2019‑2021) while substituting stepper‑driven syringe pumps for rapid 10 µL deliveries, and adding capacitive lick detection on each side.  The result is a compact, self‑contained apparatus for operant or Pavlovian paradigms in rodents.

All hardware control, task logic, and SD logging are encapsulated in a single C++ class `FED3`, defined in **TwoBottle.h / TwoBottle.cpp**.  Example sketches simply instantiate the class, select a behavioural mode, and call `run()` inside `loop()`—leaving you free to focus on experimental design rather than boilerplate.

## Key Features

* **Dual independent liquid lines** — left & right 3‑way valves driven by discrete 28BYJ‑48 steppers (or any 4‑wire unipolar motor).
* **High‑speed dispense** — configurable `dispenseRPM` and per‑side step count (`doseLeftSteps`, `doseRightSteps`) yield \~10 µL in < 250 ms.
* **Capacitive lick sensing** — Adafruit MPR121 breakout on a secondary I²C bus (`Wire2`) with IRQ handling down to 5 ms resolution.
* **Rich behavioural schedules** — free feeding, FR/PR, two‑armed bandit (80/20 & 100/0), extinction, light tracking, self‑stimulation, and timed access windows.
* **On‑board display & menu** — 144 × 168 Sharp Memory LCD provides live stats, battery meter, animated splash, and a push‑button mode selector (via nose‑poke sensors).
* **SD/CSV logging** — millisecond‑stamped events with battery, temperature/humidity (AHT20), poke/lick counts, dispense attempts, and block metadata.
* **TTL/BNC output** — optically isolate external stimulators with precise pulse width & frequency control (`pulseGenerator()`).
* **Low‑power standby** — automatic motor release and 5‑second sleep loop for prolonged battery operation.

## Hardware Overview

```
 ┌────────────────────────────────────────────────────────────┐
 │ Teensy 4.1                                               │
 │  ├── I²C0  SDA0/SCL0  → AHT20  (temp & RH)               │
 │  ├── I²C2  SDA2/SCL2  → MPR121 (lick sensor)  IRQ → Pin 9 │
 │  ├── SPI   SCK/MOSI/SS → Sharp Memory LCD                 │
 │  ├── GPIO 22/21       → Infra‑red nose‑pokes (L/R)        │
 │  ├── GPIO 16‑17‑14‑13 → Stepper LEFT  (L_IN1–4)           │
 │  ├── GPIO 36‑37‑34‑33 → Stepper RIGHT (R_IN1–4)           │
 │  ├── GPIO 15 / 35     → Motor‑driver enable (L/R)         │
 │  ├── GPIO 30          → Green status LED                 │
 │  ├── GPIO  3          → Piezo buzzer                     │
 │  └── GPIO 23          → BNC/TTL out                      │
 └────────────────────────────────────────────────────────────┘
```

A complete schematic and KiCad files will be posted in the `hardware/` directory.

## Bill of Materials

| Qty | Component                                 | Supplier/Part #        |
| :-: | ----------------------------------------- | ---------------------- |
|  1  | Teensy 4.1 (with pins)                    | PJRC                   |
|  2  | 28BYJ‑48 5 V stepper + ULN2003 driver     | Generic                |
|  1  | Adafruit MPR121 capacitive touch breakout | ADA 1982               |
|  1  | Adafruit AHT20 sensor (optional)          | ADA 4566               |
|  1  | Sharp LS013B7DH06 memory LCD 1.3 "        | Mouser 969‑LS013B7DH06 |
|  1  | Adafruit NeoPixel mini‑strip (10 GRBW)    | ADA 4369               |
|  2  | Infra‑red beam‑break sensor pairs         | Pololu 2731            |
|  1  | microSD card (≥ 1 GB, class 10)           | —                      |
|  •  | 3D‑printed enclosure & bottle mounts      | see `mechanical/`      |

## Pin‑Out Reference

All pin constants are defined in **TwoBottle.h** and reproduced here for quick look‑up:

```cpp
#define LEFT_POKE   22
#define RIGHT_POKE  21
#define LEFT_LICK    0   // MPR121 electrodes
#define RIGHT_LICK   1
#define MOTOR_ENABLE_LEFT  15
#define MOTOR_ENABLE_RIGHT 35
// … full list in header …
```

## Software Requirements

* **Arduino IDE ≥ 1.8.19** or **Arduino CLI ≥ 0.34**
* **Teensyduino ≥ 1.59** (install Board → Teensy 4.1)
* The following libraries via Library Manager:

  * *Adafruit\_GFX*, *Adafruit\_SharpMem*, *Adafruit\_NeoPixel*
  * *Adafruit\_MPR121*, *Adafruit\_AHTX0*
  * *SdFat* (set `#define SD_FAT_TYPE 3`)
  * *TimeLib*

## Installation & Flashing

```bash
# 1. Clone the repo
$ git clone https://github.com/Pigbythesea/TwoBottle_FeedingDevice.git

# 2. Open the example sketch
#    File → Examples → TwoBottle → FreeFeeding

# 3. Tools → Board: "Teensy 4.1"  |  Tools → USB Type: "Serial"
# 4. Sketch → Upload (or `arduino-cli compile --upload`)
```

After flashing, the LCD splash will appear.  Hold **both** nose‑pokes > 1.5 s to enter the device‑number & clock set menu.  Brief taps on the left or right poke decrement/increment the value, respectively.

## Runtime Modes

`FEDmode` is stored on the SD card (`FEDmode.csv`) and re‑loaded at boot.  The default firmware exposes up to **12 preset modes**; long‑press either poke on the splash to cycle, then release to start.

| Mode | Behaviour         | Notes                                             |
| ---- | ----------------- | ------------------------------------------------- |
| 0    | Free feeding      | Unlimited licks ⇒ dispense                        |
| 1    | FR‑1              | One valid poke required per dispense              |
| 4    | Progressive ratio | `FR` doubles after each earned drop               |
| 6    | Light tracking    | NeoPixel cues the active side                     |
| 11   | Timed feeding     | Active only between `timedStart`–`timedEnd` hours |

Custom paradigms can be added by editing the `SelectMode()` switch in **TwoBottle.cpp**.

## Data Logging

Each event appends a line to `FED###_MMDDYYNN.CSV` on the microSD.  Columns include time‑stamp (to ms), battery voltage, left/right motor turns, lick & poke counts, inter‑pellet interval, and any auxiliary temperature/humidity.  Headers are written once at file creation via `writeHeader()`.

To parse the CSV in Python:

```python
import pandas as pd

log = pd.read_csv('FED001_08072500.CSV', parse_dates=['MM:DD:YYYY hh:mm:ss:ms'])
log['elapsed_s'] = (log['MM:DD:YYYY hh:mm:ss:ms'] - log['MM:DD:YYYY hh:mm:ss:ms'][0]).dt.total_seconds()
```

## Calibrating Dispense Volume

1. Fill syringes with water and prime the lines.
2. Place a tared micro‑balance under the spout.
3. In **Calibration.ino** set `doseLeftSteps` (or right) to an initial guess and upload.
4. Trigger 10 spins via serial `FeedLeft(steps)`; weigh the dispensed mass.
5. Adjust the step count proportionally: `new = old × (target_mass / measured_mass)`.
6. Repeat until a single dispense weighs 10 ± 0.5 mg.

## Extending the Library

* Add new behavioural schedules by subclassing **FED3** or writing wrapper sketches.
* `serviceLicks()` demonstrates low‑latency IRQ handling; similar patterns can read rotary encoders or EMG.
* `pulseGenerator()` supports closed‑loop optogenetics—modify it for patterned trains.
* The motor interface accepts any `Stepper`‑compatible driver; simply re‑map `L_IN*` / `R_IN*` pins.

## License

**Creative Commons Attribution‑ShareAlike 4.0 International**
© 2025 Zihuan Zhang — forked from **FED3** (GPL‑3.0).  You are free to use, remix, and distribute, provided you give appropriate credit and distribute derivatives under the same license.

## Acknowledgements

This project stands on the shoulders of open‑source giants:

* **Lex Kravitz & Eric Lin** — original FED3 concept and codebase.
* **Adafruit Industries** — breakout boards and driver libraries.
* **PJRC** — the formidable Teensy platform.

*If you use TwoBottle in published work, please cite the FED3 paper (Krutz et al., 2021) and credit this repository.*
