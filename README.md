# SAHEL-CUBESAT: An Experimental Educational CubeSat 
# Project work in progress

This project aims to develop a complete experimental CubeSat for educational purposes. It is based on an STM32H7 MCU for data handling and uses two CC1200 radios for communication. The software is developed using a Zephyr-like RTOS.

<table>  <tr>    <td align="center">      <img src="images/Squelette.png" width="250"><br>      <b>Mechanical Frame</b>    </td>    <td align="center">      <img src="images/Assemblage_Tr.png" width="250"><br>      <b>CubeSat Assembly with PCBs</b>    </td>    <td align="center">      <img src="images/Assemblage.png" width="250"><br>      <b>Complete CubeSat Assembly</b>    </td>  </tr></table>

## Table of Contents

- [Hardware](#hardware)
  - [Structure](#structure)
  - [Electronics](#electronics)
    - [Radio and OBC PCB](#radio-and-obc-pcb)
    - [Solar Panel and Battery PCB](#solar-panel-and-battery-pcb)

- [Software](#software)
  - [System Logic](#system-logic)
  - [Implementation](#implementation)

- [Ground Station Communication](#ground-station-communication)

---

## Hardware

### Structure

The structure was designed according to the specifications using [Onshape](https://cad.onshape.com?utm_source=chatgpt.com). All printable components can be found in this [directory](hardware/structure/).
<table>  <tr>    <td align="center">      <img src="images/faces_xy.png" width="250"><br>      <b>Mechanical faces X+ X- Y+ Y-</b>    </td>    <td align="center">      <img src="images/faces_xy.png" width="250"><br>      <b>Mechanical faces Z+ Z-</b>    </td>    <td align="center">      <img src="images/solar_pan.png" width="250"><br>      <b> Solar panel</b>    </td>  </tr></table>

---

### Electronics

KiCad is used for the electronic design. The hardware is divided into three parts. The first part includes the radio communication system and the OBC (On-Board Computer) for the CubeSat. The second part consists of a board integrating several sensors and a solar MPPT controller IC.

#### Radio and OBC PCB : Available in: [hardware/sahel_pcb_main](hardware/sahel_pcb_main/)

<table>
  <tr>
    <td align="center">
      <img src="images/Diagramme.png" width="650"><br>
      <b>Architectural Diagram of the Radio and OBC PCB</b>
    </td>
    <td align="center">
      <img src="images/main_pcb-bg.png" width="350"><br>
      <b>Radio and OBC PCB</b>
    </td>
  </tr>
</table>


---

##### Components

###### MCU

**STM32H7B0VBTx**  
High-performance Arm Cortex-M7 MCU with DSP and DP-FPU, 128 KB Flash, 1,376 KB SRAM, up to 280 MHz.

---

###### FRAM — FM24CL64B

| Parameter        | Value            |
|-----------------|------------------|
| Memory          | 64-Kbit (8K × 8) |
| I2C Address      | `0b1010000`        |
| SDA (I2C3)       | PC9              |
| SCL (I2C3)       | PA8              |
| Write Protect    | PC5              |

###### Sensors

**Accelerometer, Gyroscope, and Magnetometer: LSM9DS1**  
**Barometer: BMP388**

| Parameter            | Value        |
|---------------------|-------------|
| I2C Address (ACC/GYRO) | `0b1101011` |
| I2C Address (MAG)      | `0b0011110` |
| I2C Address (BARO)     | `0b1110111` |
| SDA (I2C4)             | PD13        |
| SCL (I2C4)             | PD12        |
| INT_ACC/GYRO           | PE8         |
| DRDY_MAG               | PE7         |
| DRDY_BARO              | PB2         |
---

###### RTC & Temperature Sensor

**RTC: RV-8803-C7**  
**TEMP: TMP100**

| Parameter        | Value            |
|-----------------|------------------|
| RTC I2C Address  | `0b0110010`        |
| TEMP I2C Address | `0b1001000`        |
| SDA (I2C1)       | PB7              |
| SCL (I2C1)       | PB6              |
| RTC_INT          | PB5              |
| RTC_EVI          | PB4              |

---

###### CAN Interface: TCAN334G

| Signal     | Pin  |
|-----------|------|
| CAN1_TX    | PD1  |
| CAN1_RX    | PD0  |
| CAN_SHDN   | PA15 |
| CAN_STBY   | PD2  |

---

###### RS-485 Interface: ISL3170E

| Signal      | Pin  |
|------------|------|
| UART2_RX    | PD6  |
| UART2_TX    | PD5  |
| UART2_EN    | PD4  |

---
###### Radio Interface: CC1200

The CC1200 device is a fully integrated single-chip radio transceiver designed for high performance at very low-power and low-voltage operation in cost-effective wireless systems frequency bands at 164–192 MHz, 274–320 MHz, 410–480 MHz, and 820–960 MHz.

**UHF(TX/RX) 420-470 MHz**

| Signal      | Pin  |
|------------|------|
| UHF_SCK(SPI1)    | PA5  |
| UHF_MISO(SPI1)     | P6  |
| UHF_MOSI(SPI1)     | P7  |
| UHF_CS(SPI1)     | PA4  |
| UHF_GPIO2    | PC5  |
| UHF_GPIO3    | PC4  |

**VHF(TX/RX) 164-192 MHz**

| Signal      | Pin  |
|------------|------|
| UHF_SCK(SPI4)    | PE12  |
| UHF_MISO(SPI4)     | PE13  |
| UHF_MOSI(SPI4)     | PE14  |
| UHF_CS(SPI4)     | PE11  |
| UHF_GPIO2    | PB10  |
| UHF_GPIO3    | PE14  |


#### Solar Panel and Battery PCB

<table>
  <tr>
    <td align="center">
      <img src="images/solar_pcb.png" width="350"><br>
      <b> Solar charger PCB </b>      
    </td>
    <td align="center">
      <img src="images/batt_pcb.png" width="350"><br>
      <b> Battery charger PCB </b>
    </td>
  </tr>
</table>

In this project, the BQ24650 solar charger is used together with an INA219A current sensor for photovoltaic (IPV) monitoring.

| Parameter                  | Value               |
|---------------------------|---------------------|
| Input Voltage (Vin)       | 9V – 24V            |
| MPPT Voltage (VMPPT)      | 9V                  |
| Charge Voltage            | 8.2V                |
| Charge Current            | 1.2A                |
| Precharge Current         | 125mA               |
| Termination Current       | 125mA               |
| INA219A I2C Address       | `0b1000010`         |
| SDA (I2C4)                | PD13                |
| SCL (I2C4)                | PD12                |

---

### Battery Pack & Protection

The system uses four RS PRO Li-Ion batteries configured in a 2S2P arrangement with the following specifications:

| Parameter                          | Value                |
|-----------------------------------|----------------------|
| Nominal Voltage                   | 3.60V                |
| Capacity                          | 3500mAh              |
| Maximum Charge Voltage            | 4.20V                |
| Energy                            | 12.60Wh              |
| Operating Voltage                 | 4.20V – 2.50V        |
| Cut-Off Voltage                   | 2.50V                |
| Standard Charge Method            | CC-CV                |
| Standard Charge Current           | 0.2C (0.7A)          |
| Standard Charge Cut-Off Current   | 0.02C (0.07A)        |
| Maximum Continuous Charge Current | 1.0C (3.5A)          |

#### Standard Discharge Conditions

| Temperature Range | Discharge Current |
|------------------|------------------|
| 0°C – 10°C       | 0.1C (0.35A)     |
| 10°C – 20°C      | 0.2C (0.7A)      |
| 20°C – 45°C      | 0.5C (1.75A)     |

The battery protection and monitoring system is based on the BQ76907 battery monitor IC and an INA219A current sensor.

| Parameter              | Value                    |
|-----------------------|--------------------------|
| I2C Address (BQ76907) | `0b0001000` (default)    |
| I2C Address (INA219A) | `0b1000000`              |
| SDA (I2C1)            | PB7                      |
| SCL (I2C1)            | PB6                      |
| Battery Alert         | PE2                      |

---

## Software

The software is developed using **Zephyr RTOS**.  
The middleware architecture is described in the [Software Documentation](software/README.md).

---

## System Logic

We are developing the system based on **Zephyr RTOS**, using a **multi-threaded architecture** combined with **Zephyr zbus** for internal communication.

The application is structured around multiple independent threads that communicate through the **zbus message bus system**.

### Key concepts:
- Each module acts as a **zbus publisher and/or subscriber**
- Data is exchanged through **well-defined channels**
- This avoids tight coupling between components
- Improves scalability and modularity of the system

### Benefits of using zbus:
- Decoupled architecture between threads
- Clean publish/subscribe communication model
- Easier debugging and tracing of data flow
- Better maintainability for complex systems

---

## Ground Station Communication

The ground station software is **not yet implemented**.  
The current plan is to develop it using either:
- **Qt-based applications** (for a custom GUI control station)
- or existing **SDR software tools** for signal reception and analysis

---

## Communication Mechanism

The communication between the system and the ground station is designed around a structured and layered protocol stack.

### 1. Physical & Link Layer
The system uses the **AX.25 protocol** as the primary data link layer.

AX.25 provides:
- reliable packet framing
- addressing capability (callsigns)
- error detection (CRC)
- compatibility with amateur radio / SDR systems

---

### 2. Packet Structure

All exchanged data is encapsulated in AX.25 frames containing:
- payload data
- message type identifier
- optional sequence number
- checksum (handled by AX.25 layer)

---

### 3. Command System

The ground station can send **commands** to the embedded system.

Commands are structured as:
- command ID
- optional parameters
- target module (if applicable)

Examples of command types:
- system control (start / stop / reset)
- configuration update
- request sensor data
- debug / diagnostics requests

---

### 4. Internal Routing (zbus integration)

Incoming and outgoing messages are bridged into the internal **zbus architecture**.

Flow:
- AX.25 frame received → decoded → converted to internal message
- message published on **zbus channel**
- relevant modules subscribe and react

---

### 5. Response System

The system responds using structured messages:
- ACK / NACK for command validation
- telemetry data packets
- event notifications
- status reports

All responses follow the same pipeline:
**zbus → encoder → AX.25 → ground station**

---

### 6. Design Goals

This architecture is designed to ensure:
- robustness over noisy RF links
- modular separation between communication and logic
- easy integration with SDR tools
- future compatibility with different ground station implementations