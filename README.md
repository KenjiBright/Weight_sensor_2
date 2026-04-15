# Weight Sensor 2

An ESP32-based weight sensing and sorting system using a load cell (HX711), LCD display, servo motor, and ESP-NOW wireless communication.

## Overview

This project implements an automated weight measurement and sorting system. It weighs objects placed on a load cell, displays the result on an LCD screen, and uses a servo motor to sort items based on their weight. The system also communicates weight data wirelessly to another ESP32 device using ESP-NOW protocol.

## Features

- **Weight Measurement**: Accurate weight sensing using HX711 load cell amplifier
- **LCD Display**: Real-time weight display on a 16x2 I2C LCD
- **Automatic Sorting**: Servo motor-controlled sorting based on weight threshold (100g)
- **Wireless Communication**: ESP-NOW protocol for sending weight data to another ESP32
- **State Machine**: Robust operation with CONNECTING, WAITING, MEASURING, and DISPLAYING states
- **Tare Function**: Serial command ('T' or 't') to zero the scale

##Images
image/image1.jpg
ima


## Hardware Requirements

| Component | Description |
|-----------|-------------|
| ESP32 Dev Board | Main microcontroller |
| HX711 | Load cell amplifier module |
| Load Cell | Weight sensor (any compatible load cell) |
| LCD 16x2 I2C | Display module (address: 0x27) |
| Servo Motor | For sorting mechanism |

## Pin Configuration

| Component | ESP32 Pin |
|-----------|-----------|
| HX711 DOUT | GPIO 4 |
| HX711 SCK | GPIO 5 |
| LCD SDA | GPIO 21 |
| LCD SCL | GPIO 22 |
| Servo | GPIO 17 |

## Installation

### Prerequisites

- [PlatformIO](https://platformio.org/) IDE or CLI
- USB cable for programming

### Steps

1. Clone this repository:
   ```bash
   git clone https://github.com/KenjiBright/Weight_sensor_2.git
   cd Weight_sensor_2
   ```

2. Open the project in PlatformIO or VS Code with PlatformIO extension

3. Update the MAC address in `src/main.cpp` to match your receiver ESP32:
   ```cpp
   uint8_t broadcastAddress[] = {0x3C, 0x71, 0xBF, 0x12, 0x34, 0x56};
   ```

4. Build and upload:
   ```bash
   pio run --target upload
   ```

5. Open Serial Monitor at 115200 baud to view debug output

## Dependencies

The following libraries are automatically installed via PlatformIO:

- [HX711](https://github.com/bogde/HX711) (v0.7.5) - Load cell amplifier library
- [LiquidCrystal_I2C](https://github.com/marcoschwartz/LiquidCrystal_I2C) (v1.1.4) - I2C LCD library
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) (v3.0.9) - Servo control for ESP32

## Usage

### Operation Flow

1. **CONNECTING**: On startup, the system attempts to connect to the receiver ESP32 via ESP-NOW
2. **WAITING**: Once connected, displays "San sang can!" (Ready to weigh) and waits for an object
3. **MEASURING**: When weight exceeds 30g, starts a 5-second measurement period
4. **DISPLAYING**: Shows final weight and sorts the object:
   - Weight > 100g: Servo rotates left (0°)
   - Weight ≤ 100g: Servo rotates right (180°)
5. After removing the object, servo returns to center (90°) and system returns to WAITING

### Serial Commands

- **'T' or 't'**: Tare (zero) the scale

### Configuration Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `TRIGGER_WEIGHT` | 30.0g | Minimum weight to start measurement |
| `REMOVE_WEIGHT` | 10.0g | Weight threshold to reset system |
| `DEAD_ZONE` | 2.0g | Noise filtering threshold |
| `MEASURE_TIME` | 5000ms | Measurement duration |
| `calibration_factor` | 401.94 | HX711 calibration factor |

## Calibration

To calibrate the load cell:

1. Upload the code and open Serial Monitor
2. Place a known weight on the scale
3. Adjust `calibration_factor` in `src/main.cpp` until the displayed weight matches
4. Re-upload the code

## Project Structure

```
Weight_sensor_2/
├── src/
│   └── main.cpp          # Main application code
├── include/              # Header files (if any)
├── lib/                  # Private libraries
├── test/                 # Test files
├── platformio.ini        # PlatformIO configuration
└── README.md             # This file
```

## License

This project is open source. Feel free to use and modify as needed.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
