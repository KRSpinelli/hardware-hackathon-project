# Smart Desk Coach — Wiring Guide

**MCU: STM32 Nucleo G474RE**

## Concept

A desk/chair device that detects whether someone is seated too long, whether their posture is slouching, and gives a physical reminder to move.

| Component | Role |
|---|---|
| MPU-6050 | Posture / leaning / movement detection |
| HC-SR04 | Detects if person is actually sitting near the chair/desk |
| MCP9808 | Environmental temperature / "heat/stuffy room" context |
| SG90 Servo | Physical alert (coach arm waves) |
| LEDs (G/Y/R) | Status: good posture → warning → get up |

---

## I2C Bus — D14 (PB9) SDA / D15 (PB8) SCL

Both MPU-6050 and MCP9808 share the same I2C bus (different addresses).

### MPU-6050 (posture/motion) — I2C addr `0x68`

| MPU-6050 Pin | Nucleo Pin | Notes |
|---|---|---|
| VCC | 3.3V | |
| GND | GND | |
| SCL | D15 (PB8) | I2C1 |
| SDA | D14 (PB9) | I2C1 |
| INT | D2 (PA10) | Timer interrupt for sampling |
| AD0 | GND | Sets address to 0x68 |

### MCP9808 (temperature) — I2C addr `0x18`

| MCP9808 Pin | Nucleo Pin | Notes |
|---|---|---|
| VDD | 3.3V | |
| GND | GND | |
| SCL | D15 (PB8) | Same I2C bus |
| SDA | D14 (PB9) | Same I2C bus |
| A0/A1/A2 | GND | All to GND = addr 0x18 |

> If using the raw MCP9808 chip (not a breakout board), add 4.7kΩ pull-up resistors from SDA and SCL to 3.3V.

---

## HC-SR04 (presence detection)

> **Critical:** ECHO outputs 5V — use a voltage divider to protect the STM32's 3.3V GPIO.

```
HC-SR04 ECHO → 1kΩ → [node] → D6 (PB10)
                               [node] → 2kΩ → GND
```

| HC-SR04 Pin | Nucleo Pin | Notes |
|---|---|---|
| VCC | 5V | |
| GND | GND | |
| TRIG | D7 (PA8) | Digital out |
| ECHO | D6 (PB10) via divider | 5V→3.3V voltage divider required |

---

## SG90 Servo (coach arm / alert)

| Servo Wire | Nucleo Pin | Notes |
|---|---|---|
| Red (VCC) | 5V | Servo needs 5V |
| Brown (GND) | GND | |
| Orange (signal) | D9 (PC7) | PWM via TIM3_CH2, 50Hz |

---

## LEDs (status indicators)

| LED | Nucleo Pin | Resistor |
|---|---|---|
| Green (good posture) | D4 (PB5) | 220Ω to GND |
| Yellow (slouch warning) | D11 (PA7) | 220Ω to GND |
| Red (get up!) | D3 (PB3) | 220Ω to GND |

---

## Power Rail Summary

```
Nucleo 3.3V  →  MPU-6050, MCP9808
Nucleo 5V    →  HC-SR04, SG90 servo
GND          →  everything
```

---

## RTOS Task → Pin Mapping

| RTOS Task | Pins Used |
|---|---|
| Posture classification | D14, D15 (I2C), D2 (INT) |
| Presence detection | D7 (TRIG), D6 (ECHO) |
| Temperature reading | D14, D15 (shared I2C) |
| LED feedback | D3, D4, D11 |
| Servo actuation | D9 (PWM) |

---

## Demo Behavior

| State | Sensor trigger | Output |
|---|---|---|
| Seated, good posture | MPU-6050 upright + HC-SR04 detects presence | Green LED |
| Slouching | MPU-6050 tilt threshold exceeded | Yellow/Red LED + servo points down |
| Seated too long (timer) | RTOS timer interrupt fires | Servo waves + LED flashes |
| Stood up | HC-SR04 detects absence | All LEDs off, timer resets |
| Hot/stuffy room | MCP9808 above threshold | Red LED + serial message "Take a movement break + cool down" |
