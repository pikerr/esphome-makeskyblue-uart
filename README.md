# makeskyblue_uart

ESPHome external component to interface MakeSkyBlue (and compatible Easun / PowMr MPPT charge controllers) via the dedicated **MiniUSB UART port** instead of hacking internal display bus wiring.

## Overview

Unlike display sniffer components that interface internal display bus wires, this component uses the standard 9600 baud TTL UART interface available on the controller's external MiniUSB socket.

### Features
- **Active Polling (Master Mode)**: ESP32 queries state every N seconds.
- **Real-Time Telemetry**:
  - Battery Voltage & Charging Current
  - Solar PV Voltage & Charging Power
  - Internal Heat-Sink Temperature
  - Total Accumulated Generation (kWh)
- **Status & Alarms**:
  - UART Connection Link status
  - MPPT Active Mode status
  - Undervoltage / Overvoltage Alarm flags
- **Remote Configuration (Read & Write)**:
  - MPPT Bulk Voltage
  - MPPT Float Voltage
  - Max Charging Current Limit
  - UVP Off & Recovery Voltages
  - Battery Type Selection (`SLA`, `LiPo`, `LiLo`, `LiFe`, `LiTo`)

---

## Hardware Connection

The external MiniUSB connector on MakeSkyBlue is **NOT USB protocol**; it exposes 3.3V/5V TTL Serial UART signals.

### MiniUSB Pinout

| MiniUSB Pin | Function | ESP32 Connection | Notes |
|:---:|:---:|:---:|:---|
| Pin 1 | VBUS / +5V | VIN / 5V | Can power ESP32 (check current limit) |
| Pin 2 | D- (TX) | ESP32 RX (e.g. GPIO16) | MakeSkyBlue TX output |
| Pin 3 | D+ (RX) | ESP32 TX (e.g. GPIO17) | MakeSkyBlue RX input |
| Pin 5 | GND | ESP32 GND | Common Ground |

> **Note:** Baud rate is `9600`, 8 data bits, 1 stop bit, no parity (8N1).

---

## Usage Example

Refer to [`makeskyblue_uart_example.yaml`](makeskyblue_uart_example.yaml) for a complete ESPHome configuration template.
