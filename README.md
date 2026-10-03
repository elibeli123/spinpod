# spinpod
Autonomous multi-wavelength optical sensing system for microbial growth experiments, developed in connection to NASA Ames. Integrates synchronized firmware, spectral sensing, LED control, and Python data acquisition for reliable, high-throughput measurements


# Micronauts: Autonomous Multi-Wavelength Optical Sensing System

## Overview

Micronauts is an autonomous, multi-wavelength optical sensing platform developed at NASA Ames Research Center for measuring microbial growth for simulated microgravity conditions. 

The system replaces a manual, low-throughput measurement process with an integrated platform capable of synchronized multi-wavelength illumination, spectral sensing, automated data acquisition, and continuous operation.

The platform combines custom optical hardware, two embedded controllers, multiplexed spectral sensors, programmable LED modules, and a Python-based graphical interface and data acquisition system.

## Key Capabilities

- Autonomous operation for 24+ hours
- 8 parallel sensor/LED modules
- 5 controlled LED illumination wavelengths
- 18-channel spectral measurements from 410–940 nm
- Automated CSV data collection
- Configurable LED brightness
- Selection of individual sensor modules for experiments
- Real-time experiment control through a graphical interface
- Strong agreement with reference instrumentation (R² > 0.99)

---

## System Architecture

The system consists of three main layers:

### 1. Optical Hardware

Each experimental module combines controlled LED illumination with an AS7265x spectral sensor.

Five illumination wavelengths are used:

- 695 nm
- 465 nm
- 850 nm
- 640 nm
- 569 nm

The AS7265x sensor records 18 spectral channels:

`410, 435, 460, 485, 510, 535, 560, 585, 610, 645, 680, 705, 730, 760, 810, 860, 900, and 940 nm`

Up to eight sensor/LED modules can be addressed independently through I²C multiplexers.

### 2. Embedded Control

The embedded system uses two microcontrollers.

**Sensor Controller**

The sensor-side controller acts as the primary experiment controller. It:

- Initializes the AS7265x spectral sensors
- Selects individual sensor modules
- Coordinates measurements
- Commands the LED controller
- Controls experiment timing
- Outputs spectral measurements over USB serial in CSV format

**LED Controller**

The LED-side controller:

- Receives commands from the sensor controller over UART
- Selects one of eight LED modules
- Controls the AW9523 LED drivers
- Turns individual LEDs on and off
- Controls global and module-specific LED brightness

The two controllers communicate over UART at **38,400 baud**.

### 3. Host Software

The host-side software is written in Python and provides:

- A graphical user interface
- Serial communication with the sensor controller
- Experiment configuration
- Sensor/module selection
- LED brightness configuration
- Experiment start/stop control
- Automatic CSV data logging
- Filtering and validation of incoming measurements

The computer communicates with the sensor controller over USB serial at **115,200 baud**.

---

# System Requirements

## Hardware Requirements

The system requires the following major hardware components:

### Embedded Controllers

- 2 Arduino-compatible microcontroller boards
  - Sensor controller
  - LED controller

The firmware assumes UART communication between the controllers using:

- D4 — RX
- D5 — TX

### Spectral Sensors

- AS7265x spectral sensor modules
- Up to 8 sensor modules supported

Each sensor provides calibrated measurements across 18 spectral channels from 410 nm to 940 nm.

### LED System

Each module supports five independently controlled LED channels corresponding to:

- 695 nm
- 465 nm
- 850 nm
- 640 nm
- 569 nm

The LED controller supports up to eight LED modules.

### LED Drivers

- AW9523 LED driver
- I²C address: `0x58`

The firmware uses five AW9523 output channels per LED module.

### I²C Multiplexers

I²C multiplexers are used to independently address the eight sensor and LED modules.

The firmware expects the multiplexer address:

`0x70`

The system supports channels `0–7`.

### Host Computer

A computer is required to:

- Run the Python control software
- Display the graphical interface
- Communicate with the sensor controller over USB serial
- Store experimental data as CSV files

---

## Firmware Requirements

The embedded firmware is written in Arduino-compatible C/C++.

### Required Arduino Libraries

The LED controller requires:

- `Wire`
- `Adafruit_AW9523`
- `SoftwareSerial`

The sensor controller requires:

- `Wire`
- `SoftwareSerial`
- `SparkFun_AS7265X`

These libraries must be installed in the Arduino development environment before compiling and uploading the firmware.

---

## Python Requirements

The host software requires Python 3.

### Python Libraries

The software uses:

- `tkinter`
- `pyserial`
- `threading`
- `pathlib`
- `time`

Most of these modules are included with standard Python installations.

`pyserial` must be installed separately:

```bash
pip install pyserial
```

Depending on the Python installation and operating system, Tkinter may also need to be installed separately.

---

## Communication Requirements

Two serial communication links are used.

### Computer → Sensor Controller

Baud rate:

```text
115200
```

This connection is used for:

- Experiment configuration
- START/STOP commands
- Status requests
- Spectral data transmission

### Sensor Controller → LED Controller

Baud rate:

```text
38400
```

This UART connection is used to send:

- LED module selection
- LED ON/OFF commands
- Brightness commands
- Initialization commands

---

## Repository Structure

```text
.
├── README.md
├── requirements.txt
│
├── firmware/
│   ├── led_controller/
│   │   └── led_controller.ino
│   │
│   └── sensor_controller/
│       └── sensor_controller.ino
│
└── software/
    ├── main.py
    ├── ui.py
    ├── controller.py
    └── serial_logger.py
```

### Firmware

`firmware/led_controller/led_controller.ino`

Controls LED module selection, LED state, and brightness through the AW9523 LED drivers.

`firmware/sensor_controller/sensor_controller.ino`

Acts as the primary embedded experiment controller. Coordinates spectral measurements, communicates with the LED controller, and outputs measurement data.

### Software

`software/main.py`

Entry point for the host application.

`software/ui.py`

Tkinter graphical interface for configuring and controlling experiments.

`software/controller.py`

Manages experiment execution, configuration, serial communication, and CSV data collection.

`software/serial_logger.py`

Implements the serial communication interface and validates incoming measurement data.

---

# Installation

## 1. Install the Embedded Firmware

Upload:

```text
firmware/led_controller/led_controller.ino
```

to the LED controller.

Upload:

```text
firmware/sensor_controller/sensor_controller.ino
```

to the sensor controller.

Before uploading, install the required Arduino libraries.

## 2. Install Python Dependencies

Install Python 3 if it is not already installed.

Then install PySerial:

```bash
pip install pyserial
```

## 3. Connect the Hardware

Connect:

1. The spectral sensors to the sensor-side I²C multiplexer.
2. The LED drivers to the LED-side I²C multiplexer.
3. The sensor and LED controllers through their UART connection.
4. The sensor controller to the host computer over USB.

## 4. Run the Application

Navigate to the software directory:

```bash
cd software
```

Run:

```bash
python main.py
```

The experiment control interface should open.

---

# Running an Experiment

From the graphical interface:

1. Select the serial port corresponding to the sensor controller.
2. Choose the output CSV filename.
3. Select which sensor systems should participate in the experiment.
4. Configure global LED brightness if required.
5. Configure individual module brightness overrides if required.
6. Start the experiment.

The software sends the configuration to the embedded controller and begins recording valid spectral measurements to the selected CSV file.

The experiment can be stopped from the graphical interface.

---

# Data Output

Measurements are stored in CSV format.

Each measurement contains:

```text
time(ms),
sensor_id,
trial,
410nm,
435nm,
460nm,
485nm,
510nm,
535nm,
560nm,
585nm,
610nm,
645nm,
680nm,
705nm,
730nm,
760nm,
810nm,
860nm,
900nm,
940nm
```

The `trial` field identifies the illumination condition, such as a baseline measurement or one of the five LED wavelengths.

---

# Results

The completed system:

- Operated autonomously for more than 24 hours
- Increased experimental throughput by 8×
- Reduced system cost by approximately 90%
- Produced measurements with R² > 0.99 compared with reference instrumentation

Development and validation focused on optical alignment, communication reliability, measurement timing, system integration, and repeatability.

---

# Project Context

This system was developed at NASA Ames Research Center to support scalable optical sensing for long-duration biological experiments.

The project involved optical system design, embedded firmware, sensor and LED multiplexing, serial communication, automated data acquisition, experimental validation, and iterative system debugging.
