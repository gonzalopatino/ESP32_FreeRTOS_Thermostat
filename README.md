# ThermostatRTOS

[![License](https://img.shields.io/badge/License-Proprietary-blue.svg)](LICENSE)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5.1-green.svg)](https://github.com/espressif/esp-idf)
[![Platform](https://img.shields.io/badge/Platform-ESP32-orange.svg)](https://www.espressif.com/en/products/socs/esp32)

**Professional Smart Thermostat Firmware**

A production-grade smart thermostat system built on ESP32 with FreeRTOS, featuring WiFi connectivity, cloud telemetry, and a responsive user interface.

---

## 🏢 About

**ThinkSense Labs** - Building intelligent IoT solutions for smart homes and buildings.

**Author:** Gonzalo Patino  
**Company:** ThinkSense Labs  
**Version:** 0.1.0  

---

## ✨ Features

- **Multi-Mode Operation**: Heat, Cool, Auto, and Off modes
- **Precision Control**: Hysteresis-based temperature regulation
- **Cloud Connectivity**: Real-time telemetry to backend server
- **WiFi Provisioning**: Secure QR code-based device setup
- **Settings Mode**: Runtime configuration via web interface
- **Watchdog Monitoring**: Robust task health supervision
- **LCD Display**: Real-time temperature and status display
- **Physical Controls**: Button-based setpoint adjustment

---

## 📁 Project Structure

```
thermostat/
├── main/                           # Application entry point
│   └── app_main.c                  # Main application bootstrap
│
├── components/
│   ├── core/                       # Core business logic
│   │   ├── include/core/
│   │   │   ├── config.h            # System configuration
│   │   │   ├── thermostat.h        # Thermostat state machine
│   │   │   ├── thermostat_config.h # Runtime configuration
│   │   │   ├── logging.h           # Logging subsystem
│   │   │   ├── error.h             # Error handling
│   │   │   ├── watchdog.h          # Task health monitoring
│   │   │   ├── provisioning.h      # Device provisioning
│   │   │   └── boot_mode.h         # Boot mode management
│   │   └── src/
│   │       ├── thermostat.c        # Control algorithm
│   │       ├── thermostat_config.c # NVS configuration
│   │       └── ...
│   │
│   ├── app_thermostat/             # Application layer (tasks)
│   │   ├── include/app/
│   │   │   ├── task_sensors.h      # Temperature acquisition
│   │   │   ├── task_control.h      # HVAC control logic
│   │   │   ├── task_display.h      # LCD UI management
│   │   │   ├── task_net.h          # Network/telemetry
│   │   │   └── setup_server.h      # Provisioning server
│   │   └── src/
│   │       └── ...
│   │
│   └── drivers_thermostat/         # Hardware abstraction layer
│       ├── include/drivers/
│       │   ├── drv_temp_sensors.h  # Temperature sensor driver
│       │   ├── drv_display.h       # LCD driver (HD44780)
│       │   └── drv_buttons.h       # Button input driver
│       └── src/
│           └── ...
│
├── docs/                           # Documentation
│   └── ARCHITECTURE.md             # System architecture
│
├── CMakeLists.txt                  # Build configuration
├── sdkconfig                       # ESP-IDF configuration
├── LICENSE                         # License file
└── README.md                       # This file
```

---

## 🔧 Hardware Requirements

| Component | Specification |
|-----------|--------------|
| MCU | ESP32-WROOM-32 |
| Temperature Sensor | AHT20 (I2C) |
| Display | 16x2 LCD (HD44780, 4-bit mode) |
| Buttons | 3x Momentary (UP, DOWN, MODE) |
| Relay Output | Heat/Cool GPIO control |

### Pin Configuration

| Function | GPIO |
|----------|------|
| I2C SDA | 21 |
| I2C SCL | 22 |
| LCD RS | 19 |
| LCD EN | 18 |
| LCD D4-D7 | 5, 17, 16, 4 |
| BTN UP | 25 |
| BTN DOWN | 26 |
| BTN MODE | 27 |
| HEAT OUT | 12 |
| COOL OUT | 33 |
| LED | 2 |

---

## 🚀 Getting Started

### Prerequisites

- [ESP-IDF v5.5.1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- Python 3.8+
- USB-to-Serial adapter

### Build & Flash

```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Build the project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

### First-Time Setup

1. Power on the device
2. Scan the QR code displayed on the LCD
3. Connect to the device's WiFi AP
4. Enter your home WiFi credentials
5. Device will connect and begin normal operation

---

## 📊 Architecture

The firmware follows a **layered architecture** pattern:

```
┌─────────────────────────────────────────────────┐
│                 Application Layer               │
│  (Tasks: Sensors, Control, Display, Network)    │
├─────────────────────────────────────────────────┤
│                   Core Layer                    │
│  (Thermostat Logic, Config, Logging, Error)     │
├─────────────────────────────────────────────────┤
│            Hardware Abstraction Layer           │
│     (Drivers: Sensors, Display, Buttons)        │
├─────────────────────────────────────────────────┤
│              FreeRTOS / ESP-IDF                 │
│         (RTOS, WiFi, NVS, HTTP Server)          │
└─────────────────────────────────────────────────┘
```

---

## 📝 Coding Standards

- **Language**: C17 (GNU extensions)
- **Style**: K&R with 4-space indentation
- **Documentation**: Doxygen-compatible comments
- **Naming**: 
  - Functions: `module_action_object()` (e.g., `thermostat_set_mode()`)
  - Types: `module_name_t` (e.g., `thermostat_state_t`)
  - Constants: `UPPER_SNAKE_CASE`
  - Static variables: `s_name`

---

## 📄 License

Copyright © 2024-2025 ThinkSense Labs. All rights reserved.

This software is proprietary and confidential. Unauthorized copying, distribution, or use is strictly prohibited.

---

## 📞 Contact

**ThinkSense Labs**  
Email: gonzalo@thinksense.io  
Website: https://thinksense.io

---

*Built with ❤️ by ThinkSense Labs*
