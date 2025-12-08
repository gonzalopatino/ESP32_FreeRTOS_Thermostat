# ThermostatRTOS System Specification

**Document Number:** TSS-001  
**Version:** 1.0  
**Date:** December 8, 2025  
**Author:** Gonzalo Patino  
**Status:** Release Candidate  

---

## Document Control

| Version | Date       | Author          | Description                     |
|---------|------------|-----------------|--------------------------------|
| 0.1     | 2024-10-01 | Gonzalo Patino  | Initial draft                  |
| 0.5     | 2024-11-15 | Gonzalo Patino  | Added hardware specifications  |
| 0.9     | 2025-01-10 | Gonzalo Patino  | Complete system architecture   |
| 1.0     | 2025-12-08 | Gonzalo Patino  | Release candidate for review   |

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Purpose and Background](#2-purpose-and-background)
3. [Scope](#3-scope)
4. [Assumptions](#4-assumptions)
5. [Constraints](#5-constraints)
6. [Limitations](#6-limitations)
7. [Tradeoffs](#7-tradeoffs)
8. [Functional Requirements](#8-functional-requirements)
9. [Non-Functional Requirements](#9-non-functional-requirements)
10. [System Specifications](#10-system-specifications)
11. [System Design and Architecture](#11-system-design-and-architecture)
12. [Detailed Design](#12-detailed-design)
13. [Testing and Verification Strategy](#13-testing-and-verification-strategy)
14. [Deployment, Maintenance, and Operating Environment](#14-deployment-maintenance-and-operating-environment)
15. [Risk Assessment](#15-risk-assessment)
16. [Glossary and References](#16-glossary-and-references)

---

## 1. Executive Summary

ThermostatRTOS is a production-grade smart thermostat firmware built on the ESP32 microcontroller platform using FreeRTOS. The system provides autonomous HVAC control with cloud connectivity, enabling users to monitor and control their home heating and cooling systems remotely.

**Key Capabilities:**
- Precision temperature monitoring with ±0.3°C accuracy
- Autonomous heating/cooling control with configurable hysteresis
- Cloud telemetry and remote configuration via companion dashboard
- Zero-touch WiFi provisioning for consumer deployment
- Real-time operating system for deterministic control response

**Target Market:** Residential HVAC control for retrofit and new installations.

**Value Proposition:** Delivers enterprise-grade reliability and cloud connectivity at consumer price points, bridging the gap between basic programmable thermostats and expensive smart home ecosystems.

---

## 2. Purpose and Background

### 2.1 Mission Statement

To create a reliable, secure, and user-friendly smart thermostat that provides homeowners with precise temperature control and energy visibility without requiring technical expertise for installation or operation.

### 2.2 Problem Statement

Existing smart thermostats fall into two categories:
1. **Premium devices** (Nest, Ecobee) with high cost ($200-300+) and ecosystem lock-in
2. **Basic programmable thermostats** lacking connectivity and remote control

There is a market gap for an affordable, open-architecture smart thermostat that provides essential smart features without premium pricing or proprietary constraints.

### 2.3 Project Context

This project is developed as the Capstone Project for Southern New Hampshire University's Computer Science program, demonstrating proficiency in:
- Embedded systems development
- Real-time operating systems
- Full-stack web development
- IoT system architecture
- Professional software engineering practices

### 2.4 Stakeholders

| Stakeholder          | Interest                                    |
|---------------------|---------------------------------------------|
| End Users           | Reliable temperature control, ease of use   |
| SNHU Faculty        | Academic evaluation of engineering competency|
| HVAC Installers     | Simple commissioning process                |
| Cloud Infrastructure| Secure, efficient data transmission         |

---

## 3. Scope

### 3.1 In Scope

| Category              | Included Items                                          |
|-----------------------|---------------------------------------------------------|
| Hardware Platform     | ESP32-WROOM-32 module                                   |
| Sensors               | AHT20 temperature/humidity sensor                       |
| Display               | HD44780 16x2 LCD (4-bit mode)                          |
| User Input            | Physical pushbuttons (UP, DOWN, MODE)                  |
| HVAC Control          | Single-stage heating and cooling relay outputs          |
| Connectivity          | 2.4 GHz WiFi (802.11 b/g/n)                            |
| Provisioning          | SoftAP-based WiFi configuration                        |
| Cloud Services        | Telemetry upload, remote configuration                  |
| Operating System      | FreeRTOS (ESP-IDF v5.5.1)                              |

### 3.2 Out of Scope

| Category              | Excluded Items                                          |
|-----------------------|---------------------------------------------------------|
| Multi-Stage HVAC      | Two-stage or variable-speed systems                    |
| Heat Pump Control     | Reversing valve, auxiliary heat logic                  |
| Bluetooth             | BLE provisioning or control                            |
| Voice Control         | Alexa, Google Assistant integration                    |
| Geofencing            | Location-based automation                              |
| Learning Algorithms   | Predictive scheduling, occupancy learning              |
| Battery Backup        | Power failure operation                                |
| HVAC Wiring Detection | Automatic wire identification                          |

### 3.3 System Boundaries

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SYSTEM BOUNDARY                               │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    ThermostatRTOS                            │    │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │    │
│  │  │ Sensors │  │ Display │  │ Buttons │  │ HVAC Relays     │ │    │
│  │  └─────────┘  └─────────┘  └─────────┘  └─────────────────┘ │    │
│  │  ┌─────────────────────────────────────────────────────────┐ │    │
│  │  │                    ESP32 Firmware                        │ │    │
│  │  └─────────────────────────────────────────────────────────┘ │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                              ▲                                       │
│                              │ WiFi                                  │
│                              ▼                                       │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    Cloud Backend                             │    │
│  │         (External - Companion System)                        │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 4. Assumptions

### 4.1 Environmental Assumptions

| ID    | Assumption                                                    |
|-------|---------------------------------------------------------------|
| A-001 | Device operates in indoor residential environment             |
| A-002 | Ambient temperature range: 0°C to 50°C                       |
| A-003 | Relative humidity: 10% to 90% non-condensing                 |
| A-004 | Device mounted on interior wall, not in direct sunlight       |

### 4.2 Infrastructure Assumptions

| ID    | Assumption                                                    |
|-------|---------------------------------------------------------------|
| A-005 | 2.4 GHz WiFi network available within range                  |
| A-006 | Internet connectivity available for cloud features            |
| A-007 | 24VAC HVAC power available (standard US residential)          |
| A-008 | Backend server operational and reachable                      |

### 4.3 User Assumptions

| ID    | Assumption                                                    |
|-------|---------------------------------------------------------------|
| A-009 | User has smartphone with camera for QR code scanning          |
| A-010 | User can connect to WiFi networks on mobile device            |
| A-011 | User understands basic thermostat operation (setpoint concept)|
| A-012 | User has access to home WiFi credentials                      |

### 4.4 HVAC System Assumptions

| ID    | Assumption                                                    |
|-------|---------------------------------------------------------------|
| A-013 | HVAC system is single-stage (on/off control)                  |
| A-014 | Separate heating and cooling equipment                        |
| A-015 | HVAC responds to relay closure within 5 seconds               |
| A-016 | No minimum off-time requirements beyond standard 3-minute delay|

---

## 5. Constraints

### 5.1 Hardware Constraints

| ID    | Constraint                          | Value/Limit                    |
|-------|-------------------------------------|--------------------------------|
| C-001 | Microcontroller                     | ESP32-WROOM-32 (fixed)         |
| C-002 | Flash Memory                        | 4 MB                           |
| C-003 | SRAM                                | 520 KB (shared with WiFi)      |
| C-004 | GPIO Count                          | Limited to 34 pins             |
| C-005 | ADC Resolution                      | 12-bit                         |
| C-006 | Operating Voltage                   | 3.3V logic                     |

### 5.2 Protocol Constraints

| ID    | Constraint                          | Specification                  |
|-------|-------------------------------------|--------------------------------|
| C-007 | WiFi Band                           | 2.4 GHz only (no 5 GHz)        |
| C-008 | I2C Speed                           | Standard mode (100 kHz)        |
| C-009 | LCD Interface                       | 4-bit parallel mode            |
| C-010 | Cloud Protocol                      | HTTP/HTTPS (no MQTT)           |

### 5.3 Regulatory Constraints

| ID    | Constraint                          | Requirement                    |
|-------|-------------------------------------|--------------------------------|
| C-011 | FCC Compliance                      | Must use certified WiFi module |
| C-012 | UL/CE Safety                        | Low-voltage isolation required |
| C-013 | Energy Star                         | Not targeted for this version  |

### 5.4 Resource Constraints

| ID    | Constraint                          | Budget                         |
|-------|-------------------------------------|--------------------------------|
| C-014 | CPU Usage (Idle)                    | < 30% average                  |
| C-015 | Memory Usage                        | < 80% of available heap        |
| C-016 | Network Bandwidth                   | < 1 KB/minute average          |
| C-017 | Power Consumption                   | < 500 mW typical               |

### 5.5 Manufacturing Constraints

| ID    | Constraint                          | Requirement                    |
|-------|-------------------------------------|--------------------------------|
| C-018 | Component Availability              | JLCPCB/LCSC stocked parts      |
| C-019 | Assembly                            | SMT + through-hole hybrid      |
| C-020 | Test Points                         | Accessible for production test |

---

## 6. Limitations

### 6.1 Functional Limitations

| ID    | Limitation                                                    |
|-------|---------------------------------------------------------------|
| L-001 | No support for multi-zone HVAC systems                        |
| L-002 | No support for heat pump reversing valve control              |
| L-003 | No support for humidifier/dehumidifier control                |
| L-004 | No support for ventilation fan control                        |
| L-005 | No scheduling or programming capability (cloud-dependent)     |
| L-006 | No offline schedule storage                                   |

### 6.2 Performance Limitations

| ID    | Limitation                                                    |
|-------|---------------------------------------------------------------|
| L-007 | Temperature update rate limited to 2-second intervals         |
| L-008 | Display refresh rate limited to 500ms                         |
| L-009 | Cloud telemetry limited to 30-second intervals minimum        |
| L-010 | WiFi reconnection may take up to 30 seconds                   |

### 6.3 Environmental Limitations

| ID    | Limitation                                                    |
|-------|---------------------------------------------------------------|
| L-011 | Not rated for outdoor installation                            |
| L-012 | Not rated for high-humidity environments (bathrooms, pools)   |
| L-013 | Not rated for industrial/commercial HVAC systems              |
| L-014 | No operation during power outages (no battery)                |

### 6.4 Security Limitations

| ID    | Limitation                                                    |
|-------|---------------------------------------------------------------|
| L-015 | No end-to-end encryption for telemetry (HTTPS only)           |
| L-016 | No hardware secure element for key storage                    |
| L-017 | Device ID based on MAC address (spoofable)                    |

---

## 7. Tradeoffs

### 7.1 Design Tradeoffs Matrix

| Decision Point           | Option A              | Option B              | Choice | Rationale                           |
|--------------------------|----------------------|----------------------|--------|-------------------------------------|
| Display Technology       | OLED (high contrast) | LCD (low cost)       | LCD    | Cost reduction, adequate visibility |
| Communication Protocol   | MQTT (efficient)     | HTTP (simple)        | HTTP   | Simpler backend, easier debugging   |
| Provisioning Method      | BLE (modern)         | SoftAP (compatible)  | SoftAP | No app required, universal support  |
| Sensor Interface         | OneWire (simple)     | I2C (accurate)       | I2C    | Better accuracy, humidity included  |
| RTOS Selection           | Zephyr (modern)      | FreeRTOS (mature)    | FreeRTOS | Better ESP-IDF integration        |
| Control Algorithm        | PID (precise)        | Hysteresis (simple)  | Hysteresis | Sufficient for HVAC, less tuning |

### 7.2 Tradeoff Analysis

#### 7.2.1 Simplicity vs. Feature Richness

**Decision:** Prioritize simplicity and reliability over feature count.

**Rationale:** 
- Core thermostat function must be bulletproof
- Additional features can be added via OTA updates
- Simpler systems have fewer failure modes
- Faster time-to-market for MVP

**Consequence:** Some advanced features (scheduling, learning) deferred to cloud or future versions.

#### 7.2.2 Cost vs. Performance

**Decision:** Target $15-20 BOM cost with adequate performance.

**Rationale:**
- Consumer price point requires low BOM
- ESP32 provides excellent value/performance ratio
- AHT20 sensor adequate for HVAC (not laboratory) applications

**Consequence:** Some premium features (e.g., high-resolution display) not included.

#### 7.2.3 Autonomy vs. Cloud Dependency

**Decision:** Core HVAC control operates fully offline; cloud enhances functionality.

**Rationale:**
- HVAC control is safety-critical; must not depend on internet
- Cloud provides convenience features (remote access, telemetry)
- Graceful degradation when cloud unavailable

**Consequence:** Some features (remote setpoint, scheduling) unavailable without cloud.

#### 7.2.4 Security vs. Usability

**Decision:** Balance security with ease of provisioning.

**Rationale:**
- Consumer devices must be easy to set up
- SoftAP provisioning is less secure than BLE but more universal
- HMAC signing provides adequate API security for residential use

**Consequence:** Provisioning window is a temporary security exposure (mitigated by timeout).

---

## 8. Functional Requirements

### 8.1 Temperature Sensing

| ID     | Requirement                                                  | Priority |
|--------|--------------------------------------------------------------|----------|
| FR-001 | System SHALL read temperature from AHT20 sensor              | Must     |
| FR-002 | System SHALL read humidity from AHT20 sensor                 | Must     |
| FR-003 | Temperature readings SHALL update every 2 seconds            | Must     |
| FR-004 | System SHALL apply configurable calibration offset           | Should   |
| FR-005 | System SHALL detect sensor failure and report error          | Must     |

### 8.2 HVAC Control

| ID     | Requirement                                                  | Priority |
|--------|--------------------------------------------------------------|----------|
| FR-010 | System SHALL control heating relay based on setpoint         | Must     |
| FR-011 | System SHALL control cooling relay based on setpoint         | Must     |
| FR-012 | System SHALL implement configurable hysteresis (0.5-3.0°C)   | Must     |
| FR-013 | System SHALL prevent simultaneous heating and cooling        | Must     |
| FR-014 | System SHALL enforce minimum 3-minute compressor off-time    | Must     |
| FR-015 | System SHALL support OFF mode (no HVAC operation)            | Must     |
| FR-016 | System SHALL support HEAT mode (heating only)                | Must     |
| FR-017 | System SHALL support COOL mode (cooling only)                | Must     |
| FR-018 | System SHALL support AUTO mode (automatic heat/cool)         | Should   |

### 8.3 User Interface

| ID     | Requirement                                                  | Priority |
|--------|--------------------------------------------------------------|----------|
| FR-020 | System SHALL display current temperature on LCD              | Must     |
| FR-021 | System SHALL display setpoint temperature on LCD             | Must     |
| FR-022 | System SHALL display current HVAC mode on LCD                | Must     |
| FR-023 | System SHALL display HVAC active state (heating/cooling)     | Must     |
| FR-024 | UP button SHALL increase setpoint by 0.5°C per press         | Must     |
| FR-025 | DOWN button SHALL decrease setpoint by 0.5°C per press       | Must     |
| FR-026 | MODE button short press SHALL cycle through HVAC modes       | Must     |
| FR-027 | MODE button 5-second hold SHALL enter Settings Mode          | Must     |
| FR-028 | Setpoint changes SHALL persist across power cycles           | Must     |

### 8.4 WiFi Provisioning

| ID     | Requirement                                                  | Priority |
|--------|--------------------------------------------------------------|----------|
| FR-030 | System SHALL enter provisioning mode on first boot           | Must     |
| FR-031 | System SHALL broadcast SoftAP with SSID "Thermostat_XXXX"    | Must     |
| FR-032 | System SHALL serve configuration web page on 192.168.4.1     | Must     |
| FR-033 | System SHALL accept WiFi SSID and password via web form      | Must     |
| FR-034 | System SHALL store WiFi credentials in NVS                   | Must     |
| FR-035 | System SHALL connect to configured WiFi on subsequent boots  | Must     |
| FR-036 | System SHALL display IP address on LCD after connection      | Must     |
| FR-037 | System SHALL timeout provisioning after 5 minutes            | Should   |

### 8.5 Settings Mode

| ID     | Requirement                                                  | Priority |
|--------|--------------------------------------------------------------|----------|
| FR-040 | Settings Mode SHALL enable concurrent AP and STA (AP+STA)    | Must     |
| FR-041 | Settings Mode SHALL serve configuration web interface        | Must     |
| FR-042 | Settings Mode SHALL allow WiFi credential updates            | Must     |
| FR-043 | Settings Mode SHALL allow thermostat parameter adjustment    | Should   |
| FR-044 | Settings Mode SHALL exit on any button press                 | Must     |
| FR-045 | Settings Mode SHALL timeout after 5 minutes of inactivity    | Should   |

### 8.6 Cloud Connectivity

| ID     | Requirement                                                  | Priority |
|--------|--------------------------------------------------------------|----------|
| FR-050 | System SHALL transmit telemetry to cloud backend             | Must     |
| FR-051 | Telemetry SHALL include: temperature, humidity, setpoint     | Must     |
| FR-052 | Telemetry SHALL include: HVAC mode, heating/cooling state    | Must     |
| FR-053 | System SHALL sign API requests with HMAC-SHA256              | Must     |
| FR-054 | System SHALL accept remote setpoint updates from cloud       | Should   |
| FR-055 | System SHALL accept remote mode changes from cloud           | Should   |
| FR-056 | System SHALL continue HVAC control if cloud unreachable      | Must     |

### 8.7 System Management

| ID     | Requirement                                                  | Priority |
|--------|--------------------------------------------------------------|----------|
| FR-060 | System SHALL support OTA firmware updates                    | Should   |
| FR-061 | System SHALL implement hardware watchdog (5-second timeout)  | Must     |
| FR-062 | System SHALL log events to serial console                    | Must     |
| FR-063 | System SHALL provide factory reset capability                | Should   |
| FR-064 | System SHALL persist configuration across power cycles       | Must     |

---

## 9. Non-Functional Requirements

### 9.1 Performance Requirements

| ID     | Requirement                                          | Target          |
|--------|------------------------------------------------------|-----------------|
| NFR-001| Control loop latency                                 | < 100 ms        |
| NFR-002| Button response time                                 | < 50 ms         |
| NFR-003| Display update latency                               | < 500 ms        |
| NFR-004| WiFi connection time (from boot)                     | < 15 seconds    |
| NFR-005| Cloud telemetry round-trip                           | < 5 seconds     |
| NFR-006| Boot time to operational                             | < 10 seconds    |

### 9.2 Reliability Requirements

| ID     | Requirement                                          | Target          |
|--------|------------------------------------------------------|-----------------|
| NFR-010| Mean Time Between Failures (MTBF)                    | > 50,000 hours  |
| NFR-011| Watchdog recovery time                               | < 10 seconds    |
| NFR-012| WiFi reconnection attempts                           | Unlimited       |
| NFR-013| NVS write endurance                                  | > 100,000 cycles|
| NFR-014| Uptime availability                                  | > 99.9%         |

### 9.3 Scalability Requirements

| ID     | Requirement                                          | Target          |
|--------|------------------------------------------------------|-----------------|
| NFR-020| Concurrent task count                                | 8 tasks         |
| NFR-021| Maximum heap usage                                   | < 80%           |
| NFR-022| Stack headroom per task                              | > 20%           |
| NFR-023| NVS partition utilization                            | < 50%           |

### 9.4 Security Requirements

| ID     | Requirement                                          | Target          |
|--------|------------------------------------------------------|-----------------|
| NFR-030| API authentication                                   | HMAC-SHA256     |
| NFR-031| Credential storage                                   | NVS encrypted   |
| NFR-032| Transport security                                   | HTTPS/TLS 1.2+  |
| NFR-033| Provisioning timeout                                 | 5 minutes max   |
| NFR-034| Failed auth lockout                                  | Not implemented |

### 9.5 Maintainability Requirements

| ID     | Requirement                                          | Target          |
|--------|------------------------------------------------------|-----------------|
| NFR-040| Code modularity                                      | Component-based |
| NFR-041| Documentation coverage                               | All public APIs |
| NFR-042| Logging verbosity                                    | Configurable    |
| NFR-043| OTA update support                                   | ESP-IDF native  |

### 9.6 Usability Requirements

| ID     | Requirement                                          | Target          |
|--------|------------------------------------------------------|-----------------|
| NFR-050| Provisioning completion time                         | < 3 minutes     |
| NFR-051| Display readability distance                         | > 1 meter       |
| NFR-052| Button tactile feedback                              | Audible click   |
| NFR-053| Error indication                                     | LCD message     |

### 9.7 Environmental Requirements

| ID     | Requirement                                          | Target          |
|--------|------------------------------------------------------|-----------------|
| NFR-060| Operating temperature                                | 0°C to 50°C     |
| NFR-061| Storage temperature                                  | -20°C to 70°C   |
| NFR-062| Operating humidity                                   | 10-90% RH       |
| NFR-063| Altitude                                             | 0-3000 meters   |

---

## 10. System Specifications

### 10.1 Electrical Specifications

| Parameter                    | Min    | Typical | Max    | Unit   |
|------------------------------|--------|---------|--------|--------|
| Supply Voltage (VCC)         | 4.75   | 5.0     | 5.25   | V      |
| Logic Level                  | 3.0    | 3.3     | 3.6    | V      |
| Operating Current            | 80     | 150     | 350    | mA     |
| Peak Current (WiFi TX)       | -      | -       | 500    | mA     |
| Sleep Current                | -      | 10      | -      | μA     |
| Relay Output Rating          | -      | -       | 30     | VAC    |
| Relay Contact Rating         | -      | -       | 10     | A      |

### 10.2 Sensor Specifications (AHT20)

| Parameter                    | Min    | Typical | Max    | Unit   |
|------------------------------|--------|---------|--------|--------|
| Temperature Range            | -40    | -       | 85     | °C     |
| Temperature Accuracy         | -      | ±0.3    | -      | °C     |
| Temperature Resolution       | -      | 0.01    | -      | °C     |
| Humidity Range               | 0      | -       | 100    | %RH    |
| Humidity Accuracy            | -      | ±2      | -      | %RH    |
| Response Time (τ63%)         | -      | 8       | -      | s      |
| I2C Address                  | -      | 0x38    | -      | hex    |

### 10.3 Display Specifications (HD44780)

| Parameter                    | Value                              |
|------------------------------|------------------------------------|
| Display Type                 | Character LCD                      |
| Characters                   | 16 columns × 2 rows               |
| Character Size               | 5×8 dot matrix                    |
| Backlight                    | LED (optional)                    |
| Interface                    | 4-bit parallel                    |
| Viewing Angle                | 6 o'clock                         |

### 10.4 Communication Specifications

| Parameter                    | Specification                      |
|------------------------------|------------------------------------|
| WiFi Standard                | IEEE 802.11 b/g/n                 |
| WiFi Frequency               | 2.4 GHz                           |
| WiFi Security                | WPA/WPA2-PSK                      |
| WiFi TX Power                | +20 dBm max                       |
| WiFi Sensitivity             | -97 dBm @ 1 Mbps                  |
| I2C Speed                    | 100 kHz (standard mode)           |
| Serial Debug                 | 115200 baud, 8N1                  |

### 10.5 Timing Specifications

| Parameter                    | Value    | Unit   |
|------------------------------|----------|--------|
| Sensor Sample Interval       | 2        | s      |
| Display Refresh Interval     | 500      | ms     |
| Telemetry Transmit Interval  | 30       | s      |
| Heartbeat Interval           | 10       | s      |
| Watchdog Timeout             | 5        | s      |
| Compressor Minimum Off Time  | 180      | s      |
| Button Debounce Time         | 50       | ms     |
| Long Press Threshold         | 5000     | ms     |

### 10.6 Thermostat Control Specifications

| Parameter                    | Min    | Default | Max    | Unit   |
|------------------------------|--------|---------|--------|--------|
| Setpoint Range               | 10.0   | 22.0    | 35.0   | °C     |
| Setpoint Increment           | -      | 0.5     | -      | °C     |
| Hysteresis                   | 0.5    | 1.0     | 3.0    | °C     |
| Calibration Offset           | -5.0   | 0.0     | +5.0   | °C     |

### 10.7 GPIO Pin Assignments

| GPIO | Function           | Direction | Notes                    |
|------|--------------------|-----------|--------------------------|
| 21   | I2C SDA            | Bidir     | AHT20 sensor             |
| 22   | I2C SCL            | Output    | AHT20 sensor             |
| 19   | LCD RS             | Output    | Register Select          |
| 18   | LCD EN             | Output    | Enable                   |
| 5    | LCD D4             | Output    | Data bit 4               |
| 17   | LCD D5             | Output    | Data bit 5               |
| 16   | LCD D6             | Output    | Data bit 6               |
| 4    | LCD D7             | Output    | Data bit 7               |
| 32   | Button UP          | Input     | Internal pull-up         |
| 33   | Button DOWN        | Input     | Internal pull-up         |
| 25   | Button MODE        | Input     | Internal pull-up         |
| 26   | Relay HEAT         | Output    | Active high              |
| 27   | Relay COOL         | Output    | Active high              |

---

## 11. System Design and Architecture

### 11.1 High-Level Block Diagram

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              ThermostatRTOS                                   │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                         APPLICATION LAYER                               │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                  │  │
│  │  │  Thermostat  │  │   Settings   │  │   Network    │                  │  │
│  │  │   Control    │  │    Mode      │  │   Manager    │                  │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘                  │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                    │                                          │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                            CORE LAYER                                   │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐          │  │
│  │  │ Logging │ │  Error  │ │Watchdog │ │  Boot   │ │  Time   │          │  │
│  │  │         │ │ Handler │ │         │ │  Mode   │ │  Util   │          │  │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘          │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                    │                                          │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                           DRIVER LAYER                                  │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                  │  │
│  │  │   Display    │  │   Buttons    │  │  Temp/Humid  │                  │  │
│  │  │  (HD44780)   │  │   (GPIO)     │  │   (AHT20)    │                  │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘                  │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                    │                                          │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                         RTOS / HAL LAYER                                │  │
│  │              FreeRTOS  │  ESP-IDF  │  Hardware Abstraction              │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                    │                                          │
└────────────────────────────────────┼─────────────────────────────────────────┘
                                     │
        ┌────────────────────────────┼────────────────────────────┐
        │                            │                            │
   ┌────┴────┐                 ┌─────┴─────┐               ┌──────┴──────┐
   │  AHT20  │                 │  HD44780  │               │   Buttons   │
   │ Sensor  │                 │    LCD    │               │  (3x GPIO)  │
   └─────────┘                 └───────────┘               └─────────────┘
```

### 11.2 FreeRTOS Task Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           FreeRTOS TASK STRUCTURE                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Priority 5 (Highest)                                                       │
│  ┌─────────────────┐                                                        │
│  │ task_control    │ ← HVAC control logic (safety-critical)                │
│  │ Stack: 4096     │                                                        │
│  └─────────────────┘                                                        │
│                                                                             │
│  Priority 4                                                                 │
│  ┌─────────────────┐  ┌─────────────────┐                                  │
│  │ task_sensors    │  │ task_buttons    │                                  │
│  │ Stack: 4096     │  │ Stack: 4096     │                                  │
│  └─────────────────┘  └─────────────────┘                                  │
│                                                                             │
│  Priority 3                                                                 │
│  ┌─────────────────┐  ┌─────────────────┐                                  │
│  │ task_display    │  │ task_net        │                                  │
│  │ Stack: 4096     │  │ Stack: 8192     │                                  │
│  └─────────────────┘  └─────────────────┘                                  │
│                                                                             │
│  Priority 2                                                                 │
│  ┌─────────────────┐                                                        │
│  │ task_heartbeat  │ ← Periodic cloud telemetry                            │
│  │ Stack: 4096     │                                                        │
│  └─────────────────┘                                                        │
│                                                                             │
│  Priority 1 (Lowest)                                                        │
│  ┌─────────────────┐                                                        │
│  │ task_logger     │ ← Background logging                                  │
│  │ Stack: 4096     │                                                        │
│  └─────────────────┘                                                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 11.3 Data Flow Diagram

```
┌─────────────┐      Temperature/Humidity      ┌─────────────┐
│   AHT20     │ ─────────────────────────────► │ task_sensors│
│   Sensor    │            I2C                 └──────┬──────┘
└─────────────┘                                       │
                                                      │ Global State
                                                      ▼
┌─────────────┐      Button Events            ┌─────────────────┐
│   GPIO      │ ─────────────────────────────►│   Thermostat    │
│   Buttons   │         Interrupts            │   State         │
└─────────────┘                               │   (Shared)      │
                                              └────────┬────────┘
                                                       │
                    ┌──────────────────────────────────┼───────────────┐
                    │                                  │               │
                    ▼                                  ▼               ▼
            ┌──────────────┐                  ┌──────────────┐  ┌──────────────┐
            │ task_control │                  │ task_display │  │ task_net     │
            └──────┬───────┘                  └──────┬───────┘  └──────┬───────┘
                   │                                 │                 │
                   ▼                                 ▼                 ▼
            ┌──────────────┐                  ┌──────────────┐  ┌──────────────┐
            │  HVAC Relays │                  │   HD44780    │  │ Cloud Backend│
            │  (GPIO)      │                  │   LCD        │  │ (HTTP)       │
            └──────────────┘                  └──────────────┘  └──────────────┘
```

### 11.4 State Machine: Boot Mode

```
                              ┌─────────────┐
                              │   POWER ON  │
                              └──────┬──────┘
                                     │
                                     ▼
                              ┌─────────────┐
                              │ Check NVS   │
                              │ for WiFi    │
                              └──────┬──────┘
                                     │
                    ┌────────────────┴────────────────┐
                    │                                 │
              No Credentials                    Has Credentials
                    │                                 │
                    ▼                                 ▼
            ┌───────────────┐                 ┌───────────────┐
            │ PROVISIONING  │                 │   NORMAL      │
            │    MODE       │                 │    MODE       │
            │               │                 │               │
            │ • SoftAP      │                 │ • STA Mode    │
            │ • Web Server  │                 │ • HVAC Ctrl   │
            │ • Config Page │                 │ • Telemetry   │
            └───────┬───────┘                 └───────────────┘
                    │
                    │ Credentials Saved
                    │
                    ▼
            ┌───────────────┐
            │    REBOOT     │
            └───────────────┘
```

### 11.5 State Machine: HVAC Control

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
                    ▼                                         │
            ┌───────────────┐                                 │
            │     IDLE      │                                 │
            │ (No HVAC On)  │                                 │
            └───────┬───────┘                                 │
                    │                                         │
     ┌──────────────┴──────────────┐                         │
     │                             │                         │
  Temp < (Setpoint - Hyst)    Temp > (Setpoint + Hyst)       │
  AND Mode = HEAT/AUTO        AND Mode = COOL/AUTO           │
     │                             │                         │
     ▼                             ▼                         │
┌─────────────┐             ┌─────────────┐                  │
│   HEATING   │             │   COOLING   │                  │
│             │             │             │                  │
│ Heat Relay  │             │ Cool Relay  │                  │
│    ON       │             │    ON       │                  │
└──────┬──────┘             └──────┬──────┘                  │
       │                           │                         │
       │ Temp >= Setpoint          │ Temp <= Setpoint        │
       │                           │                         │
       └───────────────────────────┴─────────────────────────┘
```

### 11.6 Component Interactions

| Component          | Interacts With           | Interface Type          |
|--------------------|--------------------------|-------------------------|
| task_sensors       | AHT20 Driver             | I2C                     |
| task_sensors       | Thermostat State         | Shared Memory + Mutex   |
| task_control       | Thermostat State         | Shared Memory + Mutex   |
| task_control       | GPIO (Relays)            | Direct GPIO             |
| task_buttons       | GPIO (Buttons)           | GPIO + ISR              |
| task_buttons       | Thermostat State         | Shared Memory + Mutex   |
| task_display       | LCD Driver               | 4-bit Parallel          |
| task_display       | Thermostat State         | Shared Memory (Read)    |
| task_net           | WiFi Driver              | ESP-IDF WiFi API        |
| task_net           | HTTP Client              | ESP-IDF HTTP API        |
| task_heartbeat     | Thermostat State         | Shared Memory (Read)    |
| task_heartbeat     | HTTP Client              | ESP-IDF HTTP API        |

---

## 12. Detailed Design

### 12.1 Module: Thermostat Core (`thermostat.c`)

**Purpose:** Central state management and HVAC control logic.

**Key Data Structures:**

```c
typedef struct {
    float           current_temp;       // Current temperature (°C)
    float           current_humidity;   // Current humidity (%RH)
    float           setpoint;           // Target temperature (°C)
    hvac_mode_t     mode;               // OFF, HEAT, COOL, AUTO
    bool            heating_on;         // Heating relay state
    bool            cooling_on;         // Cooling relay state
    bool            sensor_error;       // Sensor fault flag
    uint32_t        last_update_ms;     // Last state update timestamp
} thermostat_state_t;
```

**Key Functions:**

| Function                      | Description                              |
|-------------------------------|------------------------------------------|
| `thermostat_init()`           | Initialize state, load from NVS          |
| `thermostat_update_reading()` | Update current temp/humidity             |
| `thermostat_set_setpoint()`   | Set target temperature                   |
| `thermostat_set_mode()`       | Set HVAC mode                            |
| `thermostat_evaluate()`       | Run control algorithm, set outputs       |
| `thermostat_get_state()`      | Thread-safe state retrieval              |

**Control Algorithm:**

```
IF mode == OFF:
    heating_on = FALSE
    cooling_on = FALSE

ELSE IF mode == HEAT OR (mode == AUTO AND needs_heat):
    IF current_temp < (setpoint - hysteresis):
        heating_on = TRUE
    ELSE IF current_temp >= setpoint:
        heating_on = FALSE

ELSE IF mode == COOL OR (mode == AUTO AND needs_cool):
    IF current_temp > (setpoint + hysteresis):
        cooling_on = TRUE
    ELSE IF current_temp <= setpoint:
        cooling_on = FALSE
```

### 12.2 Module: Sensor Driver (`drv_temp_sensors.c`)

**Purpose:** AHT20 I2C sensor communication.

**I2C Protocol:**

```
Start → [0x38 W] → [0xAC] → [0x33] → [0x00] → Stop
Wait 80ms
Start → [0x38 R] → [Status] → [Hum H] → [Hum M] → [Hum/Temp] → [Temp M] → [Temp L] → Stop
```

**Error Handling:**

| Error Condition          | Detection Method           | Recovery Action          |
|--------------------------|----------------------------|--------------------------|
| I2C NAK                  | ESP-IDF return code        | Retry up to 3 times     |
| Invalid CRC              | Checksum mismatch          | Discard reading         |
| Timeout                  | No response in 100ms       | Reinitialize sensor     |
| Out-of-range             | Temp < -40 or > 85°C       | Flag sensor error       |

### 12.3 Module: Display Driver (`drv_display.c`)

**Purpose:** HD44780 LCD control in 4-bit mode.

**Initialization Sequence:**

```
1. Wait 50ms after power-on
2. Send 0x03 (3 times with delays)
3. Send 0x02 (4-bit mode)
4. Send 0x28 (2-line, 5x8 font)
5. Send 0x0C (Display on, cursor off)
6. Send 0x06 (Entry mode: increment)
7. Send 0x01 (Clear display)
```

**Display Layout:**

```
┌────────────────┐
│T:22.5° S:23.0°│  Line 1: Current temp, Setpoint
│HEAT    [ON]   │  Line 2: Mode, HVAC state
└────────────────┘
```

### 12.4 Module: Button Driver (`drv_buttons.c`)

**Purpose:** Debounced button input with long-press detection.

**Debounce Algorithm:**

```
ON INTERRUPT:
    Start debounce timer (50ms)

ON TIMER EXPIRE:
    IF button still pressed:
        Record press start time
        Start long-press timer (5000ms)
    ELSE:
        Ignore (bounce)

ON BUTTON RELEASE:
    IF (now - press_start) < 5000ms:
        Generate SHORT_PRESS event
    ELSE:
        (Long press already handled)

ON LONG-PRESS TIMER:
    IF button still pressed:
        Generate LONG_PRESS event
```

### 12.5 Module: Network Manager (`task_net.c`)

**Purpose:** WiFi connection management and HTTP communication.

**WiFi State Machine:**

```
DISCONNECTED → CONNECTING → CONNECTED → (IP_OBTAINED)
     ↑              │            │
     └──────────────┴────────────┘
           (Connection Lost)
```

**HTTP Telemetry Payload:**

```json
{
    "device_id": "AA:BB:CC:DD:EE:FF",
    "temperature": 22.5,
    "humidity": 45.2,
    "setpoint": 23.0,
    "mode": "HEAT",
    "heating": true,
    "cooling": false,
    "timestamp": 1702060800
}
```

### 12.6 Error Handling Strategy

| Error Type              | Detection              | Response                        |
|-------------------------|------------------------|---------------------------------|
| Sensor Failure          | I2C timeout/CRC error  | Display error, continue last value |
| WiFi Disconnect         | Event callback         | Auto-reconnect, offline mode    |
| Cloud Unreachable       | HTTP timeout           | Queue telemetry, retry later    |
| NVS Corruption          | Read failure           | Use defaults, log warning       |
| Stack Overflow          | FreeRTOS hook          | Watchdog reset                  |
| Watchdog Timeout        | Hardware WDT           | System reset                    |

---

## 13. Testing and Verification Strategy

### 13.1 Test Philosophy

The testing strategy follows the V-model approach, with verification activities corresponding to each development phase:

```
Requirements ◄──────────────────────────► Acceptance Testing
     │                                           ▲
     ▼                                           │
Architecture ◄─────────────────────────► Integration Testing
     │                                           ▲
     ▼                                           │
Detailed Design ◄──────────────────────► Unit Testing
     │                                           ▲
     ▼                                           │
Implementation ─────────────────────────────────►
```

### 13.2 Test Types

| Test Type            | Scope                           | Tools/Methods              |
|----------------------|---------------------------------|----------------------------|
| Unit Testing         | Individual functions            | Unity framework, mocks     |
| Integration Testing  | Module interactions             | Hardware-in-loop           |
| System Testing       | Complete firmware               | Manual test procedures     |
| Stress Testing       | Extended operation              | 72-hour soak test          |
| Environmental        | Temperature/humidity extremes   | Thermal chamber            |
| EMC Testing          | Electromagnetic compatibility   | Pre-compliance scan        |

### 13.3 Test Coverage Requirements

| Category                    | Target Coverage |
|-----------------------------|-----------------|
| Statement Coverage          | > 80%           |
| Branch Coverage             | > 70%           |
| Function Coverage           | 100%            |
| Requirements Traceability   | 100%            |

### 13.4 Key Test Cases

| Test ID | Requirement | Test Description                                    | Pass Criteria                    |
|---------|-------------|-----------------------------------------------------|----------------------------------|
| TC-001  | FR-001      | Read temperature from sensor                        | Value within ±1°C of reference  |
| TC-010  | FR-010      | Heating activation below setpoint                   | Relay on when T < SP - Hyst     |
| TC-013  | FR-013      | Prevent simultaneous heat/cool                      | Never both relays on            |
| TC-014  | FR-014      | Compressor short-cycle protection                   | 3-min delay enforced            |
| TC-030  | FR-030      | First-boot provisioning mode                        | AP broadcasts, web page loads   |
| TC-056  | FR-056      | Offline HVAC operation                              | Control works without cloud     |

### 13.5 Acceptance Criteria

**Minimum Viable Product (MVP) Acceptance:**

1. ✅ Device boots and displays temperature within 10 seconds
2. ✅ Setpoint adjustable via buttons, persists across power cycles
3. ✅ HVAC control activates correctly based on setpoint and hysteresis
4. ✅ WiFi provisioning completes successfully via SoftAP
5. ✅ Telemetry appears in cloud dashboard within 60 seconds
6. ✅ Device recovers from power loss without user intervention
7. ✅ 72-hour soak test passes without watchdog resets

---

## 14. Deployment, Maintenance, and Operating Environment

### 14.1 Operating Environment

| Parameter                    | Specification                    |
|------------------------------|----------------------------------|
| Installation Location        | Indoor wall-mounted              |
| Ambient Temperature          | 0°C to 50°C                     |
| Relative Humidity            | 10% to 90% (non-condensing)     |
| Altitude                     | 0 to 3000 meters                |
| Power Source                 | 24VAC HVAC transformer          |
| WiFi Environment             | 2.4 GHz, -70 dBm minimum        |

### 14.2 Installation Procedure

1. **Power Down HVAC** - Turn off circuit breaker
2. **Remove Existing Thermostat** - Note wire colors and terminals
3. **Mount Backplate** - Use provided anchors and screws
4. **Connect Wires** - Match to labeled terminals (R, C, W, Y, G)
5. **Attach Thermostat** - Snap onto backplate
6. **Power On** - Restore circuit breaker
7. **Provision WiFi** - Connect phone to "Thermostat_XXXX" AP
8. **Configure** - Enter WiFi credentials at 192.168.4.1
9. **Register Device** - Note IP address, register in dashboard
10. **Verify Operation** - Confirm temperature display and cloud connection

### 14.3 Startup Behavior

```
Power Applied
    │
    ▼
┌─────────────────┐
│ Hardware Init   │ ← GPIO, I2C, LCD initialization
│ (< 1 second)    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ NVS Load        │ ← Load saved configuration
│ (< 1 second)    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Sensor Init     │ ← AHT20 soft reset and calibration
│ (< 100 ms)      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ WiFi Connect    │ ← Connect to saved network (or AP mode)
│ (< 15 seconds)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Tasks Start     │ ← All FreeRTOS tasks begin execution
└────────┬────────┘
         │
         ▼
    OPERATIONAL
```

### 14.4 Shutdown Behavior

The device does not have a graceful shutdown sequence. On power loss:

1. NVS state is already persisted (no data loss)
2. HVAC relays de-energize (fail-safe)
3. On power restore, device boots to last known configuration

### 14.5 Firmware Updates (OTA)

**Update Process:**

1. Backend pushes update notification with firmware URL
2. Device downloads firmware to OTA partition
3. Firmware validated (SHA256 checksum)
4. Device reboots to new firmware
5. If boot fails, rollback to previous partition

**Update Requirements:**

- Minimum 50% battery equivalent (not applicable - wall-powered)
- Stable WiFi connection required
- Update size < 1.5 MB (OTA partition limit)

### 14.6 Maintenance Procedures

| Task                        | Frequency        | Procedure                         |
|-----------------------------|------------------|-----------------------------------|
| Firmware Update             | As released      | Automatic OTA                     |
| Sensor Calibration          | Annually         | Settings Mode → Calibration       |
| WiFi Reconfiguration        | As needed        | 5-second MODE hold                |
| Factory Reset               | As needed        | 10-second MODE hold (future)      |
| Physical Cleaning           | Annually         | Soft cloth, no liquids            |

---

## 15. Risk Assessment

### 15.1 Technical Risks

| ID   | Risk                              | Probability | Impact | Mitigation                           |
|------|-----------------------------------|-------------|--------|--------------------------------------|
| TR-1 | Sensor drift over time            | Medium      | Medium | Annual calibration, quality sensor   |
| TR-2 | WiFi connectivity issues          | Medium      | Low    | Offline operation, auto-reconnect    |
| TR-3 | Flash wear (NVS writes)           | Low         | High   | Write coalescing, wear leveling      |
| TR-4 | Stack overflow in tasks           | Low         | High   | Stack monitoring, adequate sizing    |
| TR-5 | I2C bus lockup                    | Low         | Medium | Timeout recovery, bus reset          |

### 15.2 Operational Risks

| ID   | Risk                              | Probability | Impact | Mitigation                           |
|------|-----------------------------------|-------------|--------|--------------------------------------|
| OR-1 | User misconfigures setpoint       | Medium      | Low    | Setpoint limits (10-35°C)            |
| OR-2 | Power outage during OTA           | Low         | High   | Dual OTA partitions, rollback        |
| OR-3 | HVAC short-cycling damage         | Medium      | High   | 3-minute compressor delay            |
| OR-4 | Incorrect wiring installation     | Medium      | High   | Clear documentation, labeled wires   |

### 15.3 Security Risks

| ID   | Risk                              | Probability | Impact | Mitigation                           |
|------|-----------------------------------|-------------|--------|--------------------------------------|
| SR-1 | Provisioning AP hijacking         | Low         | Medium | 5-minute timeout, physical access    |
| SR-2 | API replay attacks                | Low         | Low    | Timestamp in signed payload          |
| SR-3 | Credential theft from NVS         | Low         | Medium | Physical security, encrypted NVS     |
| SR-4 | Man-in-the-middle                 | Low         | Medium | HTTPS with certificate validation    |

### 15.4 Project Risks

| ID   | Risk                              | Probability | Impact | Mitigation                           |
|------|-----------------------------------|-------------|--------|--------------------------------------|
| PR-1 | Component shortage                | Medium      | Medium | Multiple vendor sources              |
| PR-2 | ESP-IDF breaking changes          | Low         | Medium | Pin to specific IDF version          |
| PR-3 | Scope creep                       | High        | Medium | Strict MVP definition                |
| PR-4 | Documentation gaps                | Medium      | Low    | Continuous documentation updates     |

### 15.5 Risk Matrix

```
              │ Low Impact │ Medium Impact │ High Impact
──────────────┼────────────┼───────────────┼─────────────
High Prob.    │            │ PR-3          │
──────────────┼────────────┼───────────────┼─────────────
Medium Prob.  │ OR-1       │ TR-1, TR-2    │ OR-3, OR-4
              │            │ PR-1, PR-4    │
──────────────┼────────────┼───────────────┼─────────────
Low Prob.     │ SR-2       │ TR-5, SR-1    │ TR-3, TR-4
              │            │ SR-3, SR-4    │ OR-2
              │            │ PR-2          │
```

---

## 16. Glossary and References

### 16.1 Glossary

| Term              | Definition                                                    |
|-------------------|---------------------------------------------------------------|
| AP                | Access Point - WiFi mode where device broadcasts network      |
| BOM               | Bill of Materials - list of components and costs              |
| ESP-IDF           | Espressif IoT Development Framework                           |
| FreeRTOS          | Free Real-Time Operating System                               |
| GPIO              | General Purpose Input/Output                                  |
| HVAC              | Heating, Ventilation, and Air Conditioning                    |
| Hysteresis        | Dead-band around setpoint to prevent rapid cycling            |
| I2C               | Inter-Integrated Circuit - two-wire serial bus                |
| ISR               | Interrupt Service Routine                                     |
| MVP               | Minimum Viable Product                                        |
| NVS               | Non-Volatile Storage - ESP32 flash key-value store            |
| OTA               | Over-The-Air - wireless firmware update                       |
| Setpoint          | Target temperature for HVAC control                           |
| SoftAP            | Software Access Point - ESP32 WiFi AP mode                    |
| STA               | Station mode - ESP32 connects to existing WiFi                |
| Telemetry         | Sensor and state data transmitted to cloud                    |
| WDT               | Watchdog Timer - hardware reset on software hang              |

### 16.2 References

**Standards:**

| Reference                | Description                                    |
|--------------------------|------------------------------------------------|
| IEEE 15288:2015          | Systems and software engineering lifecycle     |
| ISO 12207:2017           | Software lifecycle processes                   |
| INCOSE SE Handbook       | Systems engineering best practices             |
| IEC 61508                | Functional safety (reference only)             |

**Technical Documentation:**

| Reference                | Description                                    |
|--------------------------|------------------------------------------------|
| ESP32 Technical Manual   | Espressif ESP32 datasheet and reference        |
| ESP-IDF Programming Guide| Espressif official development documentation   |
| AHT20 Datasheet          | ASAIR temperature/humidity sensor              |
| HD44780 Datasheet        | Hitachi LCD controller                         |
| FreeRTOS Reference       | FreeRTOS API and kernel documentation          |

**Project Documentation:**

| Document                 | Location                                       |
|--------------------------|------------------------------------------------|
| README.md                | `/README.md`                                   |
| Architecture Guide       | `/docs/ARCHITECTURE.md`                        |
| System Specification     | `/docs/SYSTEM_SPECIFICATION.md` (this document)|
| Source Code              | `/components/`, `/main/`                       |

---

## Document Approval

| Role                | Name             | Signature        | Date       |
|---------------------|------------------|------------------|------------|
| Author              | Gonzalo Patino   |                  | 2025-12-08 |
| Technical Reviewer  |                  |                  |            |
| Project Advisor     |                  |                  |            |

---

*End of Document*

**Document ID:** TSS-001  
**Classification:** Academic / SNHU Capstone  
**Distribution:** SNHU Faculty Only
