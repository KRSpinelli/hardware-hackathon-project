# hardware-hackathon-project
# Smart Desk Coach 🪑

A bare-metal embedded system for the STM32 Nucleo-G474RE that monitors 
posture, sitting duration, and room temperature — giving real-time physical 
feedback to encourage healthier desk habits.

---

## 💡 Concept

Clip the MPU-6050 to your body, press the blue button to set your 
good-posture baseline, and the device continuously monitors:

- **Are you slouching?** → Yellow/Red LED + servo arm reacts
- **Been sitting too long?** → Servo waves, LED flashes
- **Room too hot?** → Red LED + serial alert
- **Stood up?** → All LEDs off, timer resets

---

## 🔧 Hardware

| Component | Role |
|---|---|
| STM32 Nucleo-G474RE | Main MCU (170 MHz, bare-metal C) |
| MPU-6050 | Posture & presence detection (I2C) |
| MCP9808 | Environmental temperature sensor (I2C) |
| HC-SR04 | Ultrasonic presence detection |
| SG90 Servo | Physical alert — coach arm waves |
| Green LED (PB5) | Good posture |
| Yellow LED (PA7) | Slouch warning |
| Red LED (PB3) | Get up / too hot |

---

## 🚦 States

| State | Meaning | Output |
|---|---|---|
| `S_AWAY` | MPU not detected on body | All LEDs off |
| `S_NEED_BASELINE` | Worn but no baseline set | Press blue button |
| `S_GOOD` | Good posture | 🟢 Green LED |
| `S_SLOUCH` | Drifted from baseline | 🟡 Yellow LED + servo |
| `S_TOO_LONG` | Seated > 30 minutes | 🔴 Red LED + servo waves |
| `S_HOT` | Room temp > 28°C | 🔴 Red LED + serial alert |

---

## 📊 Evidence-Based Thresholds

| Constant | Value | Justification |
|---|---|---|
| `HOT_THRESH_dC` | 28.0°C | CIBSE Guide A upper office tolerance limit; ASHRAE 55-2023 comfort zone ceiling |
| `TOO_LONG_MS` | 30 minutes | NHS, HSE & Mayo Clinic guidelines; confirmed by 2019 Cochrane review (PMC6646952) |

> Full justification in [`THRESHOLDS.md`](./THRESHOLDS.md)

---

## 📌 Pin Map

| Signal | Nucleo Pin | GPIO |
|---|---|---|
| I2C1 SCL | D15 | PB8 |
| I2C1 SDA | D14 | PB9 |
| MPU-6050 INT | D2 | PA10 |
| HC-SR04 TRIG | D7 | PA8 |
| HC-SR04 ECHO | D6 | PB10 |
| Servo PWM | D9 | PC7 |
| Green LED | D4 | PB5 |
| Yellow LED | D11 | PA7 |
| Red LED | D3 | PB3 |
| Debug UART TX | — | PA2 |
| Blue Button | — | PC13 |

> Full wiring details in [`WIRING.md`](./WIRING.md)

---

## 🛠️ Build & Flash

```bash
chmod +x flash.sh
./flash.sh
Requires arm-none-eabi-gcc and st-flash installed.

📁 File Structure
File	Description
main.c	Full firmware — sensors, scheduler tasks, state machine
sched.h	Tiny cooperative scheduler (SysTick 1ms tick)
stm32g474.h	Minimal bare-metal register map
startup.c	Vector table, Reset_Handler, SysTick
link.ld	Linker script (512K flash, 128K RAM)
flash.sh	Build + flash script
WIRING.md	Full hardware wiring guide
THRESHOLDS.md	Evidence-based threshold justifications

🔁 How to Use
Wire up hardware per WIRING.md
Flash firmware with ./flash.sh
Open serial monitor at 115200 baud
Clip MPU-6050 to your back/chest
Sit in good posture → press blue button to set baseline
Watch the LEDs — the coach is now active!
