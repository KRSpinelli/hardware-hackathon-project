# Plan: Connect MPU-6050 to Nucleo-G474RE

## Context

This is the **Smart Desk Coach** project. The MPU-6050 is already defined in
`WIRING.md` as the posture/motion sensor. The plan below covers:

1. Hardware wiring (confirmed from docs)
2. STM32CubeIDE / HAL project setup
3. I2C peripheral configuration
4. MPU-6050 driver source files
5. Integration into `main.c`

---

## 1. Hardware Wiring (no changes needed — already in WIRING.md)

| MPU-6050 Pin | Nucleo-G474RE Pin | STM32 GPIO | Notes |
|---|---|---|---|
| VCC | CN6 pin 4 (3.3 V) | — | 3.3 V rail |
| GND | CN6 pin 6 (GND) | — | Common ground |
| SCL | CN5 pin 10 (D15) | **PB8** | I2C1_SCL |
| SDA | CN5 pin 9 (D14) | **PB9** | I2C1_SDA |
| INT | CN9 pin 3 (D2) | **PA10** | Data-ready interrupt (optional) |
| AD0 | GND | — | I2C address = **0x68** |

> **Pull-ups:** Most MPU-6050 breakout boards include 4.7 kΩ pull-ups on SDA/SCL.
> If using a bare chip, add 4.7 kΩ from SDA and SCL to 3.3 V.

> **Confirmed from UM2505 Table 15/16:** PB8 = I2C1_SCL (ARD_D15), PB9 = I2C1_SDA (ARD_D14).

---

## 2. Key MPU-6050 Registers (from RM-MPU-6000A)

| Register (hex) | Name | Purpose |
|---|---|---|
| 0x6B | PWR_MGMT_1 | Wake from sleep; select clock source |
| 0x19 | SMPLRT_DIV | Sample rate divider |
| 0x1A | CONFIG | DLPF setting |
| 0x1B | GYRO_CONFIG | Gyro full-scale range |
| 0x1C | ACCEL_CONFIG | Accel full-scale range |
| 0x38 | INT_ENABLE | Enable DATA_RDY interrupt |
| 0x3B–0x48 | ACCEL/TEMP/GYRO out | 14 bytes of sensor data |
| 0x75 | WHO_AM_I | Should read 0x68 |

**Power-on note:** Device starts in sleep mode (PWR_MGMT_1 = 0x40). Must clear
the SLEEP bit before any data is valid.

---

## 3. Files to Create / Modify

```
Core/
  Inc/
    mpu6050.h          ← NEW: driver API
  Src/
    mpu6050.c          ← NEW: driver implementation
    main.c             ← MODIFY: init + read loop
```

No RTOS is assumed for this initial bring-up. Polling mode I2C (HAL_I2C_Mem_Read/Write).

---

## 4. Step-by-Step Implementation Plan

### Step 1 — STM32CubeIDE / .ioc Configuration

Open (or create) the `.ioc` file and configure:

- **I2C1**
  - Mode: `I2C`
  - Speed: Fast Mode (400 kHz) — MPU-6050 supports up to 400 kHz
  - PB8 → `I2C1_SCL`
  - PB9 → `I2C1_SDA`
  - GPIO pull: set to **No pull** (external resistors on breakout board)
  - GPIO speed: **High**

- **PA10 (optional INT pin)**
  - Mode: `GPIO_EXTI10`
  - Trigger: Rising edge (DATA_RDY is active-high pulse by default)
  - Pull: Pull-down (or none if breakout has pull-down)
  - NVIC: Enable EXTI15_10 interrupt, priority e.g. 5

- **Clock**: Ensure I2C1 kernel clock is set (PCLK1 or HSI16). At 170 MHz
  system clock, PCLK1 is typically 170 MHz — verify I2C timing registers are
  auto-calculated by CubeMX for 400 kHz.

- **USART2** (already used for VCP via ST-LINK) — keep enabled for debug printf.

Re-generate code after saving the `.ioc`.

---

### Step 2 — Create `Core/Inc/mpu6050.h`

```c
#ifndef MPU6050_H
#define MPU6050_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

/* I2C address (AD0 = GND → 0x68, shifted left 1 for HAL) */
#define MPU6050_I2C_ADDR        (0x68 << 1)

/* Register addresses */
#define MPU6050_REG_SMPLRT_DIV  0x19
#define MPU6050_REG_CONFIG      0x1A
#define MPU6050_REG_GYRO_CFG    0x1B
#define MPU6050_REG_ACCEL_CFG   0x1C
#define MPU6050_REG_INT_ENABLE  0x38
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_WHO_AM_I    0x75

/* Full-scale range selections */
typedef enum {
    GYRO_FS_250  = 0x00,   /* ±250 °/s,  131 LSB/°/s  */
    GYRO_FS_500  = 0x08,   /* ±500 °/s,  65.5 LSB/°/s */
    GYRO_FS_1000 = 0x10,   /* ±1000 °/s, 32.8 LSB/°/s */
    GYRO_FS_2000 = 0x18,   /* ±2000 °/s, 16.4 LSB/°/s */
} MPU6050_GyroFS;

typedef enum {
    ACCEL_FS_2G  = 0x00,   /* ±2g,  16384 LSB/g */
    ACCEL_FS_4G  = 0x08,   /* ±4g,   8192 LSB/g */
    ACCEL_FS_8G  = 0x10,   /* ±8g,   4096 LSB/g */
    ACCEL_FS_16G = 0x18,   /* ±16g,  2048 LSB/g */
} MPU6050_AccelFS;

/* Scaled sensor data */
typedef struct {
    float accel_x;   /* g */
    float accel_y;
    float accel_z;
    float gyro_x;    /* °/s */
    float gyro_y;
    float gyro_z;
    float temp_c;    /* °C */
} MPU6050_Data;

/* Raw sensor data (14 bytes burst) */
typedef struct {
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
    int16_t temp_raw;
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;
} MPU6050_RawData;

/* Driver handle */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    MPU6050_GyroFS    gyro_fs;
    MPU6050_AccelFS   accel_fs;
    float             gyro_lsb;   /* LSB per °/s  */
    float             accel_lsb;  /* LSB per g    */
} MPU6050_Handle;

/* API */
HAL_StatusTypeDef MPU6050_Init(MPU6050_Handle *dev,
                               I2C_HandleTypeDef *hi2c,
                               MPU6050_GyroFS gyro_fs,
                               MPU6050_AccelFS accel_fs);

HAL_StatusTypeDef MPU6050_ReadRaw(MPU6050_Handle *dev,
                                  MPU6050_RawData *raw);

void MPU6050_Convert(const MPU6050_Handle *dev,
                     const MPU6050_RawData *raw,
                     MPU6050_Data *out);

HAL_StatusTypeDef MPU6050_WhoAmI(MPU6050_Handle *dev, uint8_t *id);

#endif /* MPU6050_H */
```

---

### Step 3 — Create `Core/Src/mpu6050.c`

Key implementation points:

**`MPU6050_Init`**
1. Write `0x00` to `PWR_MGMT_1` (0x6B) — clear SLEEP bit, use internal 8 MHz oscillator.
   - Recommended: write `0x01` to select PLL with X-gyro reference (better stability).
2. Write `0x07` to `SMPLRT_DIV` (0x19) — sample rate = 8000/(1+7) = 1000 Hz (with DLPF off) or 1000/(1+7) = 125 Hz (with DLPF on).
3. Write `0x00` to `CONFIG` (0x1A) — DLPF disabled (260 Hz BW) for initial bring-up.
4. Write `gyro_fs` to `GYRO_CONFIG` (0x1B).
5. Write `accel_fs` to `ACCEL_CONFIG` (0x1C).
6. (Optional) Write `0x01` to `INT_ENABLE` (0x38) — enable DATA_RDY interrupt.
7. Store LSB sensitivity values in handle based on selected ranges.
8. Verify WHO_AM_I == 0x68; return `HAL_ERROR` if mismatch.

**`MPU6050_ReadRaw`**
- Use `HAL_I2C_Mem_Read` with `ACCEL_XOUT_H` (0x3B) as start register, 14 bytes burst.
- Reconstruct 16-bit signed values: `(buf[0] << 8) | buf[1]` for each axis.
- Byte order is big-endian (MSB first) per register map.

**`MPU6050_Convert`**
- `accel_x = raw->accel_x_raw / dev->accel_lsb`  (result in g)
- `gyro_x  = raw->gyro_x_raw  / dev->gyro_lsb`   (result in °/s)
- `temp_c  = raw->temp_raw / 340.0f + 36.53f`     (from datasheet §4.19)

**`MPU6050_WhoAmI`**
- Read 1 byte from register 0x75; expected value = 0x68.

**Timeout**: Use `HAL_MAX_DELAY` for initial bring-up; replace with a fixed
timeout (e.g. 10 ms) in production.

---

### Step 4 — Modify `Core/Src/main.c`

In `main()`, after `MX_I2C1_Init()`:

```c
/* --- MPU-6050 init --- */
MPU6050_Handle mpu;
if (MPU6050_Init(&mpu, &hi2c1, GYRO_FS_250, ACCEL_FS_2G) != HAL_OK) {
    Error_Handler();   /* check wiring / address */
}

/* --- Main loop --- */
MPU6050_RawData raw;
MPU6050_Data    data;

while (1) {
    if (MPU6050_ReadRaw(&mpu, &raw) == HAL_OK) {
        MPU6050_Convert(&mpu, &raw, &data);
        /* Use data.accel_x/y/z and data.gyro_x/y/z for posture logic */
        printf("Ax=%.2f Ay=%.2f Az=%.2f | Gx=%.2f Gy=%.2f Gz=%.2f | T=%.1f\r\n",
               data.accel_x, data.accel_y, data.accel_z,
               data.gyro_x,  data.gyro_y,  data.gyro_z,
               data.temp_c);
    }
    HAL_Delay(10);   /* 100 Hz polling */
}
```

Enable `printf` over UART: add `#include <stdio.h>` and retarget `_write` to
`HAL_UART_Transmit` (or enable ITM SWO in CubeIDE).

---

### Step 5 — (Optional) Interrupt-Driven Sampling via PA10

If the INT pin is wired:

1. In `.ioc`: configure PA10 as `GPIO_EXTI10`, rising edge, NVIC enabled.
2. Implement `HAL_GPIO_EXTI_Callback`:
   ```c
   void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
       if (GPIO_Pin == GPIO_PIN_10) {
           mpu6050_data_ready = 1;   /* volatile flag, read in main loop */
       }
   }
   ```
3. In main loop, poll `mpu6050_data_ready` flag instead of `HAL_Delay`.
4. **Do not call `HAL_I2C_Mem_Read` from inside the ISR** — set a flag only.

---

## 5. Verification Checklist

| Check | Expected result |
|---|---|
| WHO_AM_I register read | Returns `0x68` |
| Accel Z at rest (flat) | ≈ +1.0 g |
| Accel X, Y at rest (flat) | ≈ 0.0 g |
| Gyro X/Y/Z at rest | ≈ 0 °/s (small offset normal) |
| Temperature | Room temperature ±5 °C |
| I2C ACK on address 0x68 | No HAL_ERROR from Init |

---

## 6. Common Pitfalls

- **SLEEP bit**: MPU-6050 boots in sleep mode. Forgetting to clear PWR_MGMT_1
  results in all-zero sensor output.
- **HAL I2C address shift**: HAL expects the 7-bit address shifted left by 1
  (`0x68 << 1 = 0xD0`). Already handled in the header above.
- **Big-endian byte order**: MSB comes first in the burst read. Swap bytes
  correctly when reconstructing 16-bit values.
- **Clock stretching**: STM32G4 I2C supports it; MPU-6050 uses it. Ensure
  I2C1 timing is configured for 400 kHz (CubeMX auto-calculates this).
- **Pull-ups**: If not using a breakout board, 4.7 kΩ pull-ups to 3.3 V are
  mandatory on SDA and SCL.
- **AD0 pin**: Must be tied to GND (not floating) to fix address at 0x68.

---

## 7. File Summary

| File | Action |
|---|---|
| `Core/Inc/mpu6050.h` | **Create** — driver types and API declarations |
| `Core/Src/mpu6050.c` | **Create** — driver implementation |
| `Core/Src/main.c` | **Modify** — add MPU6050_Init call and read loop |
| `.ioc` file | **Modify** — enable I2C1 on PB8/PB9, optionally EXTI on PA10 |
| `WIRING.md` | No change needed (already correct) |
