# ThermostatRTOS System Architecture

**Author:** Gonzalo Patino  
**Company:** ThinkSense Labs  
**Version:** 0.1.0  
**Last Updated:** December 2024

---

## 1. Overview

ThermostatRTOS is a production-grade smart thermostat firmware designed for the ESP32 platform. The system implements a real-time HVAC control solution with cloud connectivity, user interface, and secure device provisioning.

### 1.1 Key Features

- **Real-time Control**: FreeRTOS-based task scheduling for deterministic behavior
- **Multi-mode HVAC**: Heat, Cool, Auto, and Off operating modes
- **Hysteresis Control**: Prevents rapid cycling with configurable dead-band
- **Cloud Telemetry**: Periodic state reporting to backend server
- **Secure Provisioning**: QR code-based WiFi setup with PIN protection
- **Persistent Storage**: NVS-based configuration persistence

---

## 2. System Architecture

### 2.1 Layered Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                        │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌───────┐ │
│  │ Sensors │ │ Control │ │ Display │ │ Network │ │Buttons│ │
│  │  Task   │ │  Task   │ │  Task   │ │  Task   │ │ Task  │ │
│  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └───┬───┘ │
├───────┼──────────┼──────────┼──────────┼──────────┼───────┤
│       │          │          │          │          │       │
│       ▼          ▼          ▼          ▼          ▼       │
│  ┌─────────────────────────────────────────────────────┐  │
│  │                     CORE LAYER                       │  │
│  │  ┌───────────┐ ┌─────────┐ ┌─────────┐ ┌──────────┐ │  │
│  │  │Thermostat │ │ Config  │ │ Logging │ │Provision │ │  │
│  │  │   Logic   │ │ Manager │ │ System  │ │ Manager  │ │  │
│  │  └───────────┘ └─────────┘ └─────────┘ └──────────┘ │  │
│  └─────────────────────────────────────────────────────┘  │
├───────────────────────────────────────────────────────────┤
│                    DRIVER LAYER (HAL)                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐    │
│  │ Temperature │  │   Display   │  │     Button      │    │
│  │   Sensor    │  │  (HD44780)  │  │    Inputs       │    │
│  └─────────────┘  └─────────────┘  └─────────────────┘    │
├───────────────────────────────────────────────────────────┤
│                   PLATFORM LAYER                           │
│  ┌──────────────────────────────────────────────────────┐ │
│  │        ESP-IDF / FreeRTOS / WiFi / NVS / I2C        │ │
│  └──────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────┘
```

### 2.2 Component Descriptions

| Layer | Component | Responsibility |
|-------|-----------|----------------|
| Application | task_sensors | Temperature acquisition and sampling |
| Application | task_control | HVAC control algorithm execution |
| Application | task_display | LCD UI updates and rendering |
| Application | task_net | WiFi and cloud telemetry |
| Application | task_buttons | User input handling |
| Core | thermostat | Control algorithm and state machine |
| Core | thermostat_config | Runtime configuration management |
| Core | logging | Asynchronous log queue system |
| Core | provisioning | Device setup and credential storage |
| Driver | drv_temp_sensors | AHT20 I2C sensor interface |
| Driver | drv_display | HD44780 LCD controller |
| Driver | drv_buttons | GPIO button input with ISR |

---

## 3. Task Architecture

### 3.1 Task Summary

| Task | Priority | Stack Size | Period | Description |
|------|----------|------------|--------|-------------|
| Logger | 5 | 4096 | 50ms | Log message consumer |
| Control | 5 | 4096 | Event | HVAC decision making |
| Sensors | 4 | 4096 | 500ms | Temperature sampling |
| Network | 4 | 4096 | 15s | Telemetry transmission |
| Buttons | 4 | 4096 | Event | User input processing |
| Display | 3 | 4096 | Event | LCD updates |
| Heartbeat | 3 | 4096 | 1s | System health LED |

### 3.2 Inter-Task Communication

```
┌──────────┐     sensor_sample_t     ┌──────────┐
│ SENSORS  │ ───────────────────────▶│ CONTROL  │
│   TASK   │     (Queue: 1 item)     │   TASK   │
└──────────┘                         └────┬─────┘
                                          │
                 thermostat_state_t       │
            ┌─────────────────────────────┼─────────────────┐
            │                             │                 │
            ▼                             ▼                 ▼
     ┌──────────┐                  ┌──────────┐      ┌──────────┐
     │ DISPLAY  │                  │ NETWORK  │      │  GPIO    │
     │   TASK   │                  │   TASK   │      │ Outputs  │
     └──────────┘                  └──────────┘      └──────────┘
```

---

## 4. Control Algorithm

### 4.1 Hysteresis Control

The thermostat implements a hysteresis-based control algorithm to prevent rapid on/off cycling of HVAC equipment.

```
Temperature (°C)
    ▲
    │
 SP+H ─ ─ ─ ─ ─ ─┐         ┌─ ─ ─ ─ ─ ─   (Turn OFF threshold)
    │            │         │
 SP ──────────────●─────────●──────────   (Setpoint)
    │            │         │
 SP-H ─ ─ ─ ─ ─ ─┘         └─ ─ ─ ─ ─ ─   (Turn ON threshold)
    │
    └──────────────────────────────────▶ Time
             HEAT ON    HEAT OFF
```

Where:
- **SP** = Setpoint temperature (user configurable)
- **H** = Hysteresis band (default: 0.5°C)

### 4.2 Mode Behavior

| Mode | Condition | Action |
|------|-----------|--------|
| HEAT | T < SP - H | Turn ON heater |
| HEAT | T > SP + H | Turn OFF heater |
| COOL | T > SP + H | Turn ON cooler |
| COOL | T < SP - H | Turn OFF cooler |
| AUTO | T < SP - H | Heat mode behavior |
| AUTO | T > SP + H | Cool mode behavior |
| OFF | Always | All outputs OFF |

---

## 5. Boot Sequence

```
┌─────────────────────────────────────────────────────────┐
│                    Power On / Reset                      │
└───────────────────────────┬─────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────┐
│  1. Initialize NVS Flash                                 │
│  2. Initialize Logging System                            │
│  3. Initialize Watchdog Framework                        │
└───────────────────────────┬─────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────┐
│               Check Provisioning State                   │
│          (provisioning_is_complete())                    │
└───────────────────────────┬─────────────────────────────┘
                            │
             ┌──────────────┴──────────────┐
             ▼                             ▼
    ┌────────────────┐            ┌────────────────┐
    │  SETUP MODE    │            │  NORMAL MODE   │
    │                │            │                │
    │ • Start AP     │            │ • Start WiFi   │
    │ • Show QR Code │            │ • Start Tasks  │
    │ • Run HTTP     │            │ • Run Control  │
    │   Server       │            │   Loop         │
    └────────────────┘            └────────────────┘
```

---

## 6. Data Structures

### 6.1 Core Types

```c
typedef struct {
    thermostat_mode_t   mode;         // Operating mode
    thermostat_output_t output;       // Current output state
    float               setpoint_c;   // Target temperature
    float               hysteresis_c; // Hysteresis band
    float               tin_c;        // Indoor temperature
    float               tout_c;       // Outdoor temperature
    uint64_t            timestamp_ms; // Sample timestamp
} thermostat_state_t;

typedef struct {
    float    tin_c;         // Indoor temperature (°C)
    float    tout_c;        // Outdoor temperature (°C)
    uint64_t timestamp_ms;  // Measurement time
} sensor_sample_t;
```

---

## 7. Security Considerations

### 7.1 Provisioning Security

- **PIN Protection**: 4-digit PIN required for setup
- **Limited AP Lifetime**: Setup mode auto-timeout
- **Credential Encryption**: WiFi password stored in NVS

### 7.2 API Security

- **HMAC Signing**: Telemetry payloads are signed
- **TLS Transport**: HTTPS for all cloud communication
- **API Key Storage**: Keys stored in protected NVS partition

---

## 8. Future Enhancements

1. **OTA Updates**: Over-the-air firmware updates
2. **Scheduling**: Time-based setpoint programs
3. **Multi-zone**: Support for multiple temperature zones
4. **Learning**: Adaptive scheduling based on usage patterns
5. **Geofencing**: Location-aware temperature control

---

*Document maintained by ThinkSense Labs Engineering Team*
