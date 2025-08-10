 
# STM32 + BME68x + E22-LoRa Environmental Monitoring System

**Developed by Group 5 - NordHausen Hochschule**  
*Team Members: Ali, Georgy, AbolRahman, Aswathy, Abhirami, Sern*

## Overview

This project implements an environmental monitoring system using:
- **STM32G0B1RE** microcontroller (Nucleo board)
- **BME68x** environmental sensor (temperature, humidity, pressure, gas)
- **E22-xxxT30D** LoRa wireless module
- **Dual UART** communication (CLI and LoRa)

## Pin Connections

### STM32 Nucleo Board Pinout

```
┌─────────────────────────────────────┐
│          STM32G0B1RE NUCLEO         │
│                                     │
│  ┌─────┐                            │
│  │ USB │ (ST-Link)                  │
│  └─────┘                            │
│                                     │
│ Left Side (CN7)   Right Side (CN10) │
│────────────────   ──────────────────│ 
│  PC10 [ ]           [ ] PC11        │
│  PC12 [ ]           [ ] PD2         │
│  VDD  [●]───────────[ ] E5V         │
│  BOOT0[ ]           [ ] GND         │
│  NC   [ ]           [ ] NC          │
│  NC   [ ]           [ ] IOREF       │
│  PA13 [ ]           [●] RESET       │
│  PA14 [ ]           [ ] +3V3        │
│  PA15 [ ]           [●] +5V         │
│  GND  [●]───────────[●] GND         │
│  PB7  [ ]           [●] GND         │
│  PC13 [ ]           [ ] VIN         │
│  PC14 [ ]                           │
│  PC15 [ ]           CN9             │
│  PF0  [ ]           ─────           │
│  PF1  [ ]           PA0 [ ]         │
│  VBAT [ ]           PA1 [ ]         │
│  PC2  [●] GOIPIN    PA4 [ ]         │
│  PC3  [ ]           PB0 [ ]         │
│                     PC1 [ ]         │
│  CN8                PC0 [ ]         │
│  ─────                              │
│  PC9  [ ]                           │
│  PC8  [ ]                           │
│  PC6  [ ]  ┌──────────────┐         │
│  PC5  [●]──┤ UART1_RX     │         │
│  PC4  [●]──┤ UART1_TX     │         │
│  PA12 [ ]  └──────────────┘         │
│  PA11 [ ]  ┌──────────────┐         │
│  PB8  [●]──┤ I2C1_SCL     │         │
│  PB9  [●]──┤ I2C1_SDA     │         │
│  PB2  [ ]  └──────────────┘         │
│  PB1  [ ]                           │
│  PB15 [ ]  ┌──────────────┐         │
│  PB14 [ ]  │ E22 Control  │         │
│  PB13 [ ]  ├──────────────┤         │
│  PB12 [ ]  │ M0 ─► PB8    │         │
│  PB11 [ ]  │ M1 ─► PB3    │         │
│  PB10 [ ]  └──────────────┘         │
│  PB9  [ ]                           │
│  PC8  [●]───M1                      │
│  PC3  [●]───M0                      │
│  PA3  [●]──┤ UART2_RX (CLI)│        │
│  PA2  [●]──┤ UART2_TX (CLI)│        │
│  PA10 [ ]  └───────────────┘        │
│  PA9  [ ]                           │
│  NC   [ ]                           │
│  GND  [●]                           │
│                                     │
└─────────────────────────────────────┘
```

### Connection Diagram

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   STM32     │     │   BME68x    │     │  E22 LoRa   │
│             │     │             │     │             │
│ PC4 (TX1)───┼─────┼─────────────┼─────┤ RXD         │
│ PC5 (RX1)───┼─────┼─────────────┼─────┤ TXD         │
│             │     │             │     │             │
│ PB11 (SDA)──┼─────┤ SDA         │     │             │
│ PB12 (SCL)──┼─────┤ SCL         │     │             │
│             │     │             │     │             │
│ PB8 (M0)────┼─────┼─────────────┼─────┤ M0 (GND)    │
│ PB3 (M1)────┼─────┼─────────────┼─────┤ M1 (unplug) │
│             │     │             │     │             │
│ 5V──────────┼─────┤ VCC         ├─────┤ VCC         │
│ GND─────────┼─────┤ GND         ├─────┤ GND         │
│             │     │             │     │             │
└─────────────┘     └─────────────┘     └─────────────┘
```──

### Detailed Pin Connections

#### BME68x Sensor (I2C)
| BME68x Pin | STM32 Pin | Function  |
|------------|-----------|-----------|
| VCC        | 5V        | Power     |
| GND        | GND       | Ground    |
| SCL        | PB12      | I2C Clock |
| SDA        | PB11      | I2C Data  |
| SDO        | GND/VCC   | I2C Addr  |


*Note: SDO to GND = 0x76, SDO to VCC = 0x77*

#### E22 LoRa Module
| E22 Pin | STM32 Pin | Function       |
|---------|-----------|----------------|
| VCC     | 5V        | Power (5)      |
| GND     | GND       | Ground         |
| TXD     | PC5       | UART1_RX       |
| RXD     | PC4       | UART1_TX       |
| M0      | PB8       | Mode Select 0  |
| M1      | PB3       | Mode Select 1  |
| AUX     | NC        | Not Connected  |

#### UART Connections
| Interface | STM32 Pins | Function        | Settings      |
|-----------|------------|-----------------|---------------|
| UART2     | PA2/PA3    | CLI (to PC)     | 115200, 8N1   |
| UART1     | PC4/PC5    | LoRa Module     | 9600, 8N1     |

## E22 Module Operating Modes

| Mode         | M1 | M0 | Description                         |
|--------------|----|----|-------------------------------------|
| Normal       | 0  | 0  | Transparent transmission mode       |
| WOR Transmit | 0  | 1  | Wake-on-Radio transmitting mode     |
| WOR Receive  | 1  | 0  | Wake-on-Radio receiving mode        |
| Configuration| 1  | 1  | Parameter setting mode              |

## User Manual

### Getting Started

1. **Hardware Setup**
   - Connect all components according to the pin diagram
   - Power the system via USB (ST-Link)
   - Ensure E22 module has adequate power supply (3.3V, >100mA)

2. **Software Requirements**
   - STM32CubeIDE or compatible ARM toolchain
   - PuTTY or similar terminal emulator
   - Two terminal windows (one for CLI, one for LoRa monitoring)

3. **Terminal Configuration**
   - **CLI Terminal (UART2)**: COM port, 115200 baud, 8N1
   - **LoRa Terminal (UART1)**: COM port, 9600 baud, 8N1

### Command Reference

| Command | Description | Example |
|---------|-------------|---------|
| `help` or `start` | Display help menu | `help` |
| `temp` | Read temperature from BME68x | `temp` |
| `hum` | Read humidity from BME68x | `hum` |
| `pres` | Read pressure from BME68x | `pres` |
| `all` | Read all sensor values | `all` |
| `sum n1 n2` | Calculate sum of two numbers | `sum 10 20` |
| `lora` | Send configuration to LoRa | `lora` |
| `lora_read` | Read LoRa configuration | `lora_read` |
| `lora_set` | Set LoRa channel to 0x11 | `lora_set` |
| `lora_msg <text>` | Send text via LoRa | `lora_msg Hello World` |

### Example Session

```
========================================
       NordHausen Hochschule
  Embedded Systems - GROUP 5 Present
========================================
 Ali | Georgy | AbolRahman | Aswathy
 Abhirami | Serin
----------------------------------------
System initialized successfully!

STM32> help

========== Available Commands ==========
start/help   -> Show this help menu
temp         -> Show temperature
hum          -> Show humidity
pres         -> Show pressure
all          -> Show all sensor readings
sum n1 n2    -> Sum two integers
lora         -> Send LoRa AT+CFG command
lora_read    -> Read LoRa module config
lora_rd      -> Disable  Relay
lora_re      -> Enable  Relay
lora_set     -> Set LoRa channel to
lora_msg <text> -> Send text via LoRa
========================================

STM32> temp

Temperature: 24.35 °C

STM32> all

=== Sensor Readings ===
Temperature: 24.35 °C
Humidity: 45.67 %
Pressure: 1013.25 hPa
======================

STM32> lora_msg Weather data transmitted

[SUCCESS] Message sent via LoRa: Weather data transmitted
```

### LoRa Configuration Details

When using `lora_read`, the system displays:

```
--- Parsed LoRa Configuration ---
Address: 0x0000
NETID: 0x00
UART Baud Rate: 9600 bps
UART Parity: 8N1
Air Data Rate: 2.4k bps
Sub-Packet Size: 240 bytes
RSSI Ambient Noise: Disabled
TX Power: 22dBm
Channel: 0x17 (Frequency: 433.000 MHz)
RSSI Byte in Data: Disabled
Transmission Mode: Transparent
Repeater Mode: Disabled
LBT (Listen Before Talk): Disabled
WOR Transceiver Control: Receiver
WOR Cycle: 2000ms
Encryption Key: 0x0000
-----------------------------------
```

### Troubleshooting

1. **No BME68x Response**
   - Check I2C connections (SDA/SCL)
   - Verify I2C address (0x76 or 0x77)
   - Ensure proper pull-up resistors on I2C lines

2. **No LoRa Response**
   - Verify TX/RX connections (crossed: TX→RX, RX→TX)
   - Check M0/M1 mode pins
   - Ensure adequate power supply for E22
   - Confirm baud rate matches (9600 bps)

3. **Command Not Working**
   - Type 'help' to see available commands
   - Check command syntax
   - Ensure proper terminal settings

### LED Indicators (if available)

- **Power LED**: System powered
- **User LED**: Blinks during sensor reading
- **Error State**: System halted in Error_Handler()

## Technical Specifications

### BME68x Sensor
- Temperature: -40 to +85°C (±1.0°C)
- Humidity: 0-100% RH (±3%)
- Pressure: 300-1100 hPa (±1 hPa)
- Gas: VOC detection

### E22 LoRa Module
- Frequency: 433MHz (or your specific band)
- Power: Up to 30dBm (1W)
- Range: Up to 8km (line of sight)
- Data Rate: 0.3k-19.2k bps

### Power Requirements
- STM32: 3.3V via USB
- BME68x: 3.3V, ~1mA
- E22: 3.3V, ~100mA (TX mode)

## Safety Notes

1. **Power Supply**: Ensure stable 5 for all components
2. **Antenna**: Always connect antenna before powering E22
3. **ESD Protection**: Handle boards with care
4. **Regulatory**: Ensure LoRa operation complies with local regulations

## Version History

- **v1.0** - Initial release with basic sensor and LoRa functionality
- Dual UART support (CLI + LoRa monitoring)
- BME68x environmental sensing
- E22 LoRa configuration and messaging

---

*For additional support or questions, contact the development team at NordHausen Hochschule*
I've created a comprehensive README file for your STM32 + BME68x + E22 LoRa system. The document includes:

## Key Features:

1. **Detailed Pin Diagrams**: 
   - ASCII art representation of the STM32 Nucleo board
   - Clear connection diagrams showing how to wire the BME68x sensor and E22 LoRa module
   - Pin-by-pin connection tables

2. **Complete Command Reference**:
   - All available commands with descriptions and examples
   - Example session showing typical usage
   - LoRa configuration output format

3. **User Manual**:
   - Step-by-step getting started guide
   - Terminal configuration settings
   - Troubleshooting section for common issues

4. **Technical Details**:
   - E22 operating modes explained
   - Sensor specifications
   - Power requirements

5. **Visual Connection Guide**:
   - Box diagrams showing the three main components
   - Clear indication of which pins connect where
   - UART crossover connections (TX→RX, RX→TX)

The README is formatted in Markdown for easy viewing on GitHub or other platforms. You can save this as `README.md` in your project directory. The visual diagrams make it easy for users to understand the hardware connections without needing external circuit design software.

Would you like me to add any additional sections, such as a "Known Issues" section or more detailed protocol specifications for the E22 commands?
