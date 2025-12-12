# ThermostatRTOS

<p align="center">
  <img src="https://img.shields.io/badge/ESP--IDF-v5.5.1-brightgreen?style=for-the-badge&logo=espressif" alt="ESP-IDF"/>
  <img src="https://img.shields.io/badge/Platform-ESP32-blue?style=for-the-badge" alt="Platform"/>
  <img src="https://img.shields.io/badge/RTOS-FreeRTOS-orange?style=for-the-badge" alt="FreeRTOS"/>
  <img src="https://img.shields.io/badge/License-Proprietary-red?style=for-the-badge" alt="License"/>
</p>

<p align="center">
  <strong>Production-Grade Smart Thermostat Firmware</strong><br>
  <em>Built on ESP32 with FreeRTOS for reliable HVAC control</em>
</p>

---

## Overview

ThermostatRTOS is a professional embedded firmware for smart thermostat applications. It provides precise temperature control with hysteresis-based regulation, real-time cloud telemetry, and a user-friendly interface with LCD display and physical button controls.

**Author:** Gonzalo Patino
**Company:** Southern New Hampshire University     
**Version:** 0.1.0

---

## Key Features

| Feature | Description |
|---------|-------------|
| **Multi-Mode Control** | Heat, Cool, Auto, and Off operating modes |
| **Hysteresis Regulation** | Configurable ±0.5°C deadband prevents relay chatter |
| **Local API Server** | HTTP REST API for remote configuration (setpoint, hysteresis, mode) |
| **WiFi Provisioning** | Captive portal for initial WiFi setup, displays IP on LCD |
| **QR Code Commissioning** | Dashboard generates QR code with device IP for phone-based API key provisioning |
| **Cloud Telemetry** | HTTP POST to backend every 15 seconds with signed payloads |
| **Automatic IP Reporting** | Device IP address included in telemetry for dashboard auto-detection |
| **Settings Mode** | Hold MODE 5s for runtime WiFi/API reconfiguration via web UI |
| **NVS Persistence** | Setpoint, credentials, and config survive power cycles |
| **Watchdog Monitoring** | Task health supervision with automatic recovery |
| **Real-Time Display** | 16x2 LCD showing temperature, setpoint, mode, and IP address |

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                      APPLICATION LAYER                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌────────┐ │
│  │ Sensors  │ │ Control  │ │ Display  │ │ Network  │ │Buttons │ │
│  │   Task   │ │   Task   │ │   Task   │ │   Task   │ │  Task  │ │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └───┬────┘ │
├───────┼────────────┼────────────┼────────────┼───────────┼──────┤
│       │            │     CORE LAYER          │           │      │
│       ▼            ▼            ▼            ▼           ▼      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Thermostat Engine  │  Config (NVS)  │  Logging Queue  │    │
│  │  Provisioning       │  Boot Mode     │  Error Handler  │    │
│  └─────────────────────────────────────────────────────────┘    │
├──────────────────────────────────────────────────────────────────┤
│                 HARDWARE ABSTRACTION LAYER                       │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐        │
│  │  drv_temp     │  │  drv_display  │  │  drv_buttons  │        │
│  │  (AHT20/I2C)  │  │  (HD44780)    │  │  (GPIO IRQ)   │        │
│  └───────────────┘  └───────────────┘  └───────────────┘        │
├──────────────────────────────────────────────────────────────────┤
│              ESP-IDF v5.5.1 / FreeRTOS Kernel                    │
│         WiFi • NVS • HTTP Server • I2C • GPIO • SNTP            │
└──────────────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
thermostat/
├── main/
│   ├── app_main.c              # Entry point, boot mode orchestration
│   └── CMakeLists.txt
│
├── components/
│   ├── core/                   # Business logic layer
│   │   ├── include/core/
│   │   │   ├── config.h        # Central configuration (pins, timing, etc.)
│   │   │   ├── thermostat.h    # Control algorithm interface
│   │   │   ├── thermostat_config.h  # Runtime config (setpoint, hysteresis)
│   │   │   ├── provisioning.h  # WiFi/API key storage
│   │   │   ├── boot_mode.h     # NORMAL vs SETUP mode selection
│   │   │   ├── logging.h       # Async logging queue
│   │   │   ├── error.h         # Error handling
│   │   │   ├── watchdog.h      # Task health monitoring
│   │   │   └── timeutil.h      # SNTP time sync
│   │   └── src/
│   │       ├── thermostat.c    # Hysteresis control logic
│   │       ├── thermostat_config.c  # NVS-backed configuration
│   │       ├── provisioning.c  # Credential storage
│   │       ├── boot_mode.c     # Boot mode state machine
│   │       ├── logging.c       # Queue-based logging
│   │       ├── error.c         # Fatal/non-fatal error handling
│   │       ├── watchdog.c      # Watchdog stubs
│   │       └── timeutil.c      # SNTP initialization
│   │
│   ├── app_thermostat/         # Application tasks
│   │   ├── include/app/
│   │   │   ├── task_sensors.h  # Temperature acquisition
│   │   │   ├── task_control.h  # HVAC output control
│   │   │   ├── task_display.h  # LCD UI rendering
│   │   │   ├── task_net.h      # WiFi + telemetry
│   │   │   ├── task_buttons.h  # User input handling
│   │   │   ├── task_heartbeat.h # LED blink + alive signal
│   │   │   ├── task_logger.h   # Log consumer task
│   │   │   ├── task_common.h   # Shared queues
│   │   │   ├── setup_server.h  # Captive portal + web UI
│   │   │   └── api_server.h    # Local HTTP API for remote config
│   │   └── src/
│   │       ├── task_sensors.c  # AHT20 polling @ 500ms
│   │       ├── task_control.c  # Hysteresis + relay control
│   │       ├── task_display.c  # LCD refresh loop
│   │       ├── task_net.c      # WiFi events + HTTP POST
│   │       ├── task_buttons.c  # Debounce + long-press detection
│   │       ├── task_heartbeat.c # Status LED
│   │       ├── task_logger.c   # Colored console output
│   │       ├── task_common.c   # Queue initialization
│   │       ├── setup_server.c  # SoftAP + DNS + HTTP provisioning
│   │       └── api_server.c    # REST API: GET/POST /api/config
│   │
│   └── drivers_thermostat/     # Hardware abstraction
│       ├── include/drivers/
│       │   ├── drv_temp_sensors.h
│       │   ├── drv_display.h
│       │   └── drv_buttons.h
│       └── src/
│           ├── drv_temp_sensors.c  # AHT20 I2C driver
│           ├── drv_display.c       # HD44780 4-bit driver
│           └── drv_buttons.c       # GPIO interrupt handler
│
├── docs/
│   └── ARCHITECTURE.md         # Detailed design documentation
│
├── CMakeLists.txt              # ESP-IDF project file
├── sdkconfig                   # Menuconfig settings
├── LICENSE                     # MIT License
└── README.md                   # This file
```

---

## Hardware Configuration

### Bill of Materials

| Component | Part Number | Description |
|-----------|-------------|-------------|
| MCU | ESP32-WROOM-32 | Dual-core 240MHz, WiFi+BT |
| Temp Sensor | AHT20 | I2C, ±0.3°C accuracy |
| Display | 1602A | 16x2 character LCD (HD44780) |
| Buttons | Momentary SPST | 3x tactile switches |
| Relay Module | 5V 2-channel | Heat/Cool output control |

### GPIO Pin Mapping

| Function | GPIO | Notes |
|----------|------|-------|
| **I2C SDA** | 21 | AHT20 sensor |
| **I2C SCL** | 22 | 100kHz clock |
| **LCD RS** | 19 | Register select |
| **LCD EN** | 18 | Enable strobe |
| **LCD D4** | 5 | Data nibble |
| **LCD D5** | 17 | Data nibble |
| **LCD D6** | 16 | Data nibble |
| **LCD D7** | 4 | Data nibble |
| **BTN UP** | 25 | Increase setpoint |
| **BTN DOWN** | 26 | Decrease setpoint |
| **BTN MODE** | 27 | Cycle mode / Settings (5s hold) |
| **HEAT OUT** | 12 | Heater relay |
| **COOL OUT** | 33 | Cooler relay |
| **LED** | 2 | Heartbeat indicator |

---

## Getting Started

### Prerequisites

- **ESP-IDF v5.5.1** - [Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- **Python 3.8+** - For ESP-IDF tools
- **USB-Serial Adapter** - CP2102 or similar

### Build and Flash

```bash
# Clone the repository
git clone https://github.com/gonzalopatino/ESP32_FreeRTOS_Thermostat.git
cd thermostat

# Set up ESP-IDF environment (Windows)
%IDF_PATH%\export.bat

# Set up ESP-IDF environment (Linux/macOS)
. $IDF_PATH/export.sh

# Build the firmware
idf.py build

# Flash to ESP32 (adjust COM port)
idf.py -p COM6 flash

# Monitor serial output
idf.py -p COM6 monitor

# Build + Flash + Monitor in one command
idf.py -p COM6 flash monitor
```

### First Boot: WiFi Provisioning

1. **Power on** the device - LCD shows "Setup Mode" and broadcasts a WiFi AP
2. **Connect** to the ESP32's WiFi AP (e.g., `THERMO_XXXX`) using the PIN shown on LCD
3. **Enter WiFi credentials** via the captive portal web page
4. **Device connects** to your home WiFi and displays its **IP address** on the LCD
5. **Register on Dashboard** - Go to the web dashboard and register a new device
6. **Enter IP address** - Input the device's IP address shown on LCD into the registration form
7. **Generate QR code** - Click "Update QR Code" to generate the commissioning QR
8. **Scan with phone** - Scan the QR code with your phone to automatically commission the device with API key

### Settings Mode (Runtime Reconfiguration)

Hold the **MODE button for 5 seconds** to enter Settings Mode:
- Device enters AP+STA mode (stays connected to WiFi while broadcasting AP)
- LCD displays the AP name and IP address
- Connect to the AP and access the web interface to:
  - Change WiFi SSID/password
  - Update API key
  - Perform factory reset
- Press any button to exit and restart

### Normal Operation

- **Temperature Display**: Current temp and setpoint on LCD
- **UP/DOWN Buttons**: Adjust setpoint in 0.5°C steps (15-28°C range)
- **MODE Button**: Short press cycles Heat→Cool→Off→Auto
- **Settings Mode**: Hold MODE for 5 seconds to enter web configuration

---

## Configuration

All compile-time settings are in `components/core/include/core/config.h`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `THERMOSTAT_SETPOINT_C` | 22.0 | Default setpoint (°C) |
| `THERMOSTAT_HYSTERESIS_C` | 0.5 | Deadband (±°C) |
| `THERMOSTAT_SP_MIN_C` | 15.0 | Minimum setpoint |
| `THERMOSTAT_SP_MAX_C` | 28.0 | Maximum setpoint |
| `PERIOD_SENSORS_MS` | 500 | Sensor polling rate |
| `TELEMETRY_PERIOD_MS` | 15000 | Cloud upload interval |

---

## Telemetry API

The device sends JSON telemetry to a configurable backend:

```json
{
  "device_serial": "SN-ESP32-THERO-004",
  "temperature_c": 22.5,
  "humidity_pct": 45.0,
  "setpoint_c": 22.0,
  "hysteresis_c": 0.5,
  "mode": "HEAT",
  "output": "ON",
  "device_ip": "192.168.1.100",
  "uptime_sec": 3600
}
```

**Endpoint:** `POST /api/telemetry/ingest/`  
**Authentication:** HMAC-SHA256 signed requests

**Note:** The `device_ip` field enables the dashboard to auto-detect the device's local IP address for the Remote Configuration feature.

---

## Local API Server

The ESP32 runs a local HTTP server that allows remote configuration from the web dashboard or any HTTP client on the local network.

### Endpoint: `GET /api/config`

Retrieve current thermostat configuration.

**Response:**
```json
{
  "setpoint_c": 22.0,
  "hysteresis_c": 0.5,
  "mode": "HEAT"
}
```

### Endpoint: `POST /api/config`

Update thermostat configuration. All fields are optional.

**Request:**
```json
{
  "setpoint_c": 23.5,
  "hysteresis_c": 0.8,
  "mode": "AUTO"
}
```

**Mode Values:** `OFF`, `HEAT`, `COOL`, `AUTO`

**Response:**
```json
{
  "status": "ok",
  "setpoint_c": 23.5,
  "hysteresis_c": 0.8,
  "mode": "AUTO"
}
```

**Error Response:**
```json
{
  "status": "error",
  "message": "Invalid mode: INVALID"
}
```

### CORS Support

The API server includes CORS headers to allow requests from the web dashboard:
- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, POST, OPTIONS`
- `Access-Control-Allow-Headers: Content-Type`

---

## Development

### Code Style

- **Language**: C17 (GNU extensions)
- **Formatting**: 4-space indentation, K&R braces
- **Comments**: Doxygen-compatible (`@brief`, `@param`, `@return`)
- **Naming Conventions**:
  - Functions: `module_verb_noun()` (e.g., `thermostat_set_mode()`)
  - Types: `module_name_t` (e.g., `thermostat_state_t`)
  - Macros: `UPPER_SNAKE_CASE`
  - Static vars: `s_` prefix
  - Global vars: `g_` prefix

### Task Architecture

| Task | Priority | Stack | Period | Role |
|------|----------|-------|--------|------|
| Logger | 5 | 4KB | 50ms | Console output |
| Control | 5 | 4KB | Event | Relay control |
| Sensors | 4 | 4KB | 500ms | AHT20 polling |
| Network | 4 | 4KB | 15s | Telemetry POST |
| Buttons | 4 | 4KB | Event | Input handling |
| Display | 3 | 4KB | 200ms | LCD refresh |
| Heartbeat | 3 | 4KB | 1s | LED blink |

---

## License

```
Proprietary License

Copyright (c) 2024-2025 Gonzalo Patino
All Rights Reserved.

This software is submitted as part of the Capstone Project at Southern New
Hampshire University (SNHU). Access is granted solely to authorized SNHU
faculty and staff for academic evaluation purposes.
```

See [LICENSE](LICENSE) for full terms and conditions.

---

## Contact

**Gonzalo Patino**  
Southern New Hampshire University  
Capstone Project  

📧 gonzalopatino@snhu.edu  
💻 https://github.com/gonzalopatino

---

<p align="center">
  <em>Built for the Capstone Project at Southern New Hampshire University</em>
</p>
