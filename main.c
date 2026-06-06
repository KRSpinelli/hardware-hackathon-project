/* ============================================================
 * Smart Desk Coach — STM32 Nucleo-G474RE
 * MPU-6050 clipped to user's body for posture + presence tracking.
 *
 * Pin map:
 *   I2C1:   SCL = PB8 (D15), SDA = PB9 (D14)   MPU6050@0x68, MCP9808@0x18
 *   MPU INT: PA10 (D2)
 *   HC-SR04: TRIG = PA8 (D7), ECHO = PB10 (D6, via voltage divider)
 *   Servo:  PC7 (D9) = TIM3_CH2 PWM 50Hz
 *   LEDs:   Green PB5 (D4), Yellow PA7 (D11), Red PB3 (D3)
 *   Button: PC13 (Nucleo blue user button, active LOW) — sets posture baseline
 *   Debug UART: USART2 PA2 (TX) -> ST-LINK VCP @115200
 *
 * Posture flow:
 *   1. Clip MPU-6050 to body, press blue button → captures baseline orientation.
 *   2. Device continuously compares live accel readings to baseline.
 *   3. Drift beyond threshold → bad posture (yellow/red LED, servo reacts).
 *   4. Return to baseline → good posture (green LED).
 *   5. Presence detected by accel magnitude: ~1g = clipped on body.
 * ============================================================ */
#include <stdint.h>
#include "stm32g474.h"
#include "sched.h"
#include "audio.h"

/* ---------- pin helpers ---------- */
static void gpio_mode(GPIO_TypeDef *p, int pin, int mode) {
    p->MODER &= ~(3U << (pin*2));
    p->MODER |=  ((uint32_t)mode << (pin*2));
}
static void gpio_af(GPIO_TypeDef *p, int pin, int af) {
    p->AFR[pin>>3] &= ~(0xFU << ((pin&7)*4));
    p->AFR[pin>>3] |=  ((uint32_t)af << ((pin&7)*4));
}
static void gpio_set(GPIO_TypeDef *p, int pin, int on) {
    p->BSRR = on ? (1U<<pin) : (1U<<(pin+16));
}
static int gpio_read(GPIO_TypeDef *p, int pin) { return (p->IDR >> pin) & 1U; }

/* ---------- LEDs ---------- */
#define LED_G_PORT GPIOB
#define LED_G_PIN  5
#define LED_Y_PORT GPIOA
#define LED_Y_PIN  7
#define LED_R_PORT GPIOB
#define LED_R_PIN  3

static void leds_init(void) {
    gpio_mode(LED_G_PORT, LED_G_PIN, 1);
    gpio_mode(LED_Y_PORT, LED_Y_PIN, 1);
    gpio_mode(LED_R_PORT, LED_R_PIN, 1);
}
static void leds_set(int g, int y, int r) {
    gpio_set(LED_G_PORT, LED_G_PIN, g);
    gpio_set(LED_Y_PORT, LED_Y_PIN, y);
    gpio_set(LED_R_PORT, LED_R_PIN, r);
}

/* ---------- Button: PC13, active LOW (Nucleo blue button) ---------- */
#define BTN_PORT GPIOC
#define BTN_PIN  13

static void btn_init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
    gpio_mode(BTN_PORT, BTN_PIN, 0);
    GPIOC->PUPDR &= ~(3U << (BTN_PIN*2));
    GPIOC->PUPDR |=  (1U << (BTN_PIN*2));   /* pull-up */
}

/* ---------- USART2 debug @115200 (16MHz) ---------- */
static void uart_init(void) {
    RCC->APB1ENR2 |= (1U<<0);
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    gpio_mode(GPIOA, 2, 2); gpio_af(GPIOA, 2, 12);
    gpio_mode(GPIOA, 3, 2); gpio_af(GPIOA, 3, 12);
    LPUART1->BRR = (256UL * 16000000UL) / 115200UL;
    LPUART1->CR1 = (1U<<3) | (1U<<2) | (1U<<0);
}
static void uart_putc(char c) { while(!(LPUART1->ISR & (1U<<7))); LPUART1->TDR = c; }
static void uart_print(const char *s) { while(*s) uart_putc(*s++); }

/* ---------- I2C1 @100kHz ---------- */
static void i2c_init(void) {
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
    gpio_mode(GPIOB, 8, 2); gpio_af(GPIOB, 8, 4);
    gpio_mode(GPIOB, 9, 2); gpio_af(GPIOB, 9, 4);
    GPIOB->OTYPER |= (1U<<8) | (1U<<9);
    GPIOB->PUPDR  &= ~((3U<<16)|(3U<<18));
    GPIOB->PUPDR  |=  (1U<<16)|(1U<<18);
    I2C1->CR1 = 0;
    I2C1->TIMINGR = 0x10420F13;
    I2C1->CR1 = (1U<<0);
}
static int i2c_write(uint8_t addr, const uint8_t *buf, int n) {
    I2C1->CR2 = ((uint32_t)addr<<1) | ((uint32_t)n<<16) | (1U<<13) | (1U<<25);
    for (int i=0;i<n;i++){
        uint32_t t=200000; while(!(I2C1->ISR & (1U<<1)) && --t);
        if(!t) return -1;
        I2C1->TXDR = buf[i];
    }
    uint32_t t=200000; while(!(I2C1->ISR & (1U<<5)) && --t);
    I2C1->ICR = (1U<<5);
    return t?0:-1;
}
static int i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, int n) {
    if (i2c_write(addr, &reg, 1) < 0) return -1;
    I2C1->CR2 = ((uint32_t)addr<<1) | ((uint32_t)n<<16) | (1U<<10) | (1U<<13) | (1U<<25);
    for (int i=0;i<n;i++){
        uint32_t t=200000; while(!(I2C1->ISR & (1U<<2)) && --t);
        if(!t) return -1;
        buf[i] = (uint8_t)I2C1->RXDR;
    }
    uint32_t t=200000; while(!(I2C1->ISR & (1U<<5)) && --t);
    I2C1->ICR = (1U<<5);
    return t?0:-1;
}

/* ---------- MPU-6050 (0x68) ---------- */
#define MPU_ADDR 0x68

static void mpu_init(void) {
    uint8_t wake[2] = {0x6B, 0x00};    /* PWR_MGMT_1 = 0 → wake */
    i2c_write(MPU_ADDR, wake, 2);
    /* DLPF: ~44Hz bandwidth to reduce noise before posture comparison */
    uint8_t dlpf[2] = {0x1A, 0x03};
    i2c_write(MPU_ADDR, dlpf, 2);
    /* Accel: ±2g range → 16384 LSB/g */
    uint8_t acfg[2] = {0x1C, 0x00};
    i2c_write(MPU_ADDR, acfg, 2);
}

static int mpu_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t d[6];
    if (i2c_read_reg(MPU_ADDR, 0x3B, d, 6) < 0) return -1;
    *ax = (int16_t)((d[0]<<8)|d[1]);
    *ay = (int16_t)((d[2]<<8)|d[3]);
    *az = (int16_t)((d[4]<<8)|d[5]);
    return 0;
}

/* ---------- Baseline capture + posture logic ---------- */

/* At ±2g range: 1g = 16384 LSB.
 * Presence window: magnitude² in [0.78g², 1.22g²].
 * Detects that gravity is being measured — i.e., the clip is on a body. */
#define MAG_SQ_LOW  161061273L   /* (0.78 * 16384)^2 */
#define MAG_SQ_HIGH 399769190L   /* (1.22 * 16384)^2 */

/* Slouch: sum of absolute axis deltas from baseline.
 * ~2800 LSB ≈ 10° at ±2g. Raise to reduce false positives. */
#define SLOUCH_THRESH 2800

static int16_t           g_base_ax    = 0;
static int16_t           g_base_ay    = 0;
static int16_t           g_base_az    = 0;
static volatile int      g_baseline_set = 0;

/* Average 10 readings over ~50ms, store as posture baseline. */
static void mpu_capture_baseline(void) {
    int32_t sax=0, say=0, saz=0;
    int count = 0;
    int16_t ax, ay, az;
    for (int i = 0; i < 10; i++) {
        if (mpu_read_accel(&ax, &ay, &az) == 0) {
            sax += ax; say += ay; saz += az;
            count++;
        }
        delay_us(5000);
    }
    if (count == 0) {
        uart_print("Baseline FAILED: no MPU readings\r\n");
        return;
    }
    g_base_ax = (int16_t)(sax / count);
    g_base_ay = (int16_t)(say / count);
    g_base_az = (int16_t)(saz / count);
    g_baseline_set = 1;
    uart_print("Baseline captured\r\n");
    /* Three green flashes = confirmed */
    for (int i = 0; i < 3; i++) {
        leds_set(1,0,0); delay_us(150000);
        leds_set(0,0,0); delay_us(150000);
    }
}

/* 1 = MPU is clipped on body (gravity magnitude in expected range) */
static int mpu_is_worn(int16_t ax, int16_t ay, int16_t az) {
    int32_t mag_sq = (int32_t)ax*ax + (int32_t)ay*ay + (int32_t)az*az;
    return (mag_sq > MAG_SQ_LOW && mag_sq < MAG_SQ_HIGH);
}

/* 1 = orientation has drifted from baseline beyond slouch threshold */
static int mpu_is_slouching(int16_t ax, int16_t ay, int16_t az) {
    if (!g_baseline_set) return 0;
    int dx = ax - g_base_ax; if (dx < 0) dx = -dx;
    int dy = ay - g_base_ay; if (dy < 0) dy = -dy;
    int dz = az - g_base_az; if (dz < 0) dz = -dz;
    return ((dx + dy + dz) > SLOUCH_THRESH);
}

/* ---------- MCP9808 (0x18) ---------- */
#define MCP_ADDR 0x18
static int mcp_read_temp_dC(int *out) {
    uint8_t d[2];
    if (i2c_read_reg(MCP_ADDR, 0x05, d, 2) < 0) return -1;
    int raw = ((d[0] & 0x1F) << 8) | d[1];
    if (d[0] & 0x10) raw -= 0x2000;
    *out = (raw * 10) / 16;
    return 0;
}

/* ---------- HC-SR04: TRIG PA8, ECHO PB10, uses TIM2 as µs counter ---------- */
static void hcsr04_init(void) {
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    gpio_mode(GPIOA, 8, 1);
    gpio_mode(GPIOB,10, 0);
    TIM2->PSC = 16-1;
    TIM2->ARR = 0xFFFFFFFF;
    TIM2->CR1 = 1;
}
static int hcsr04_read_cm(void) {
    gpio_set(GPIOA, 8, 0); delay_us(3);
    gpio_set(GPIOA, 8, 1); delay_us(10);
    gpio_set(GPIOA, 8, 0);
    uint32_t guard = 300000;
    while (!gpio_read(GPIOB,10) && --guard);
    if (!guard) return -1;
    TIM2->CNT = 0;
    guard = 300000;
    while ( gpio_read(GPIOB,10) && --guard);
    if (!guard) return -1;
    return (int)(TIM2->CNT / 58);
}

/* ---------- SG90 servo: PC7 = TIM3_CH2, 50Hz ---------- */
static void servo_init(void) {
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOCEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
    gpio_mode(GPIOC, 7, 2); gpio_af(GPIOC, 7, 2);
    TIM3->PSC = 16-1;
    TIM3->ARR = 20000-1;
    TIM3->CCMR1 = (6U<<12)|(1U<<11);
    TIM3->CCER  = (1U<<4);
    TIM3->CCR2  = 1500;
    TIM3->CR1   = 1;
}
static void servo_us(uint16_t us) { TIM3->CCR2 = us; }

/* ============================================================
 * Shared state
 * ============================================================ */
typedef enum {
    S_AWAY,          /* MPU not clipped on / not detected */
    S_NEED_BASELINE, /* Worn but no baseline yet — press blue button */
    S_GOOD,          /* Within baseline orientation */
    S_SLOUCH,        /* Drifted from baseline */
    S_TOO_LONG,      /* Seated too long without moving */
    S_HOT            /* Room temp above comfort threshold */
} state_t;

static volatile state_t  g_state     = S_AWAY;
static volatile int      g_present   = 0;
static volatile int      g_slouch    = 0;
static volatile int      g_temp_dC   = 0;
static volatile uint32_t g_seated_ms = 0;

#define TOO_LONG_MS   25000UL     /* DEMO: 25s (prod = 1800000UL / 30 min) */
#define HOT_THRESH_dC 280         /* 28.0°C */

/* ---------- TASK: button edge detect → capture baseline (every 50ms) ---------- */
static void task_button(void) {
    static int prev_btn = 1;
    int cur = gpio_read(BTN_PORT, BTN_PIN);   /* active LOW */
    if (!cur && prev_btn) {                    /* falling edge = press */
        mpu_capture_baseline();
        g_seated_ms = millis();                /* reset sit timer on recalibrate */
    }
    prev_btn = cur;
}

/* ---------- TASK: posture + presence via MPU-6050 (every 100ms) ---------- */
static void task_posture(void) {
    int16_t ax, ay, az;
    if (mpu_read_accel(&ax, &ay, &az) != 0) return;

    int was_present = g_present;
    g_present = mpu_is_worn(ax, ay, az);

    if (g_present && !was_present)
        g_seated_ms = millis();   /* just clipped on — start sit timer */

    g_slouch = g_present && mpu_is_slouching(ax, ay, az);
}

/* ---------- TASK: temperature (MCP9808, every 2000ms) ---------- */
static void task_temp(void) {
    int t;
    if (mcp_read_temp_dC(&t) == 0) g_temp_dC = t;
}

/* ---------- TASK: state machine + outputs (every 100ms) ---------- */
static void task_state(void) {
    state_t s;

    if (!g_present) {
        s = S_AWAY;
    } else if (!g_baseline_set) {
        s = S_NEED_BASELINE;
    } else if (g_temp_dC > HOT_THRESH_dC) {
        s = S_HOT;
    } else if ((millis() - g_seated_ms) > TOO_LONG_MS) {
        s = S_TOO_LONG;
    } else if (g_slouch) {
        s = S_SLOUCH;
    } else {
        s = S_GOOD;
    }
    g_state = s;

    switch (s) {
        case S_AWAY:
            leds_set(0,0,0);
            servo_us(1500);
            break;

        case S_NEED_BASELINE:
            /* Blink yellow: clip is on, press blue button to calibrate */
            leds_set(0, (millis()/400)%2, 0);
            servo_us(1500);
            break;

        case S_GOOD:
            leds_set(1,0,0);
            servo_us(1500);
            break;

        case S_SLOUCH:
            leds_set(0,1,0);
            servo_us(1000);   /* arm drops as "sit up" nudge */
            break;

        case S_TOO_LONG:
            leds_set(0,0,1);
            servo_us((millis()/400)%2 ? 2000 : 1000);   /* wave = get up */
            break;

        case S_HOT:
            leds_set(0,0,1);
            uart_print("Take a movement break + cool down\r\n");
            servo_us(1500);
            break;
    }
}

/* ---------- TASK: heartbeat debug (every 500ms) ---------- */
static void task_debug(void) {
    static const char *names[] = {"AWAY","NEED_BL","GOOD","SLOUCH","TOO_LONG","HOT"};
    GPIOA->ODR ^= (1U << 5);
    uart_print("state="); uart_print(names[g_state]);
    uart_print(" baseline="); uart_print(g_baseline_set ? "Y" : "N");
    uart_print("\r\n");
}

/* ================================================================
 * Audio: DAC1 / TIM6 / DMA1-Ch3 — 8-bit unsigned PCM @ 8000 Hz
 * PA4 = DAC1_OUT1 (analog mode, no AF needed).
 * TIM6 update event → TRGO → DAC trigger → DMA transfer.
 * DMA runs circular; task_audio stops it after clip duration elapses.
 * ================================================================ */

#define AUDIO_SAMPLE_RATE  8000UL
#define AUDIO_COOLDOWN_MS  10000UL   /* DEMO: 10s between repeats */

static const uint8_t  *const g_clips[]     = { audio_1, audio_2, audio_3 };

static uint32_t g_clip_lens_rt[3];

static volatile uint32_t g_audio_last_ms  = 0;
static volatile uint32_t g_audio_clip_ms  = 0;
static volatile int      g_audio_playing  = 0;

static void audio_init(void) {
    /* Clocks */
    RCC->AHB1ENR  |= RCC_AHB1ENR_DMA1EN;    /* DMA1EN  (bit 0) */
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;   /* TIM6EN  (bit 4) */
    RCC->APB1ENR1 |= RCC_APB1ENR1_DAC1EN;   /* DAC1EN  (bit 29) */

    /* PA4 → analog (MODER=11, no pull, no AF) */
    GPIOA->MODER  |=  (3U << (4*2));
    GPIOA->PUPDR  &= ~(3U << (4*2));

    /* TIM6: PSC=0, ARR=1999 → 16MHz/2000 = 8kHz update, MMS=010 (TRGO on update) */
    TIM6->PSC  = 0;
    TIM6->ARR  = (uint32_t)(16000000UL / AUDIO_SAMPLE_RATE) - 1U;  /* 1999 */
    TIM6->CR2  = TIM6_CR2_MMS_UPDATE;   /* MMS = 010 → TRGO on update */
    TIM6->CR1  = TIM6_CR1_ARPE;         /* ARPE=1, CEN stays 0 */

    /* DAC1: TEN1=1, TSEL1=0000 (TIM6 TRGO in G4 = 0b0000), DMAEN1=1, EN1=1 */
    DAC1->CR = DAC_CR_EN1
             | DAC_CR_TEN1
             | DAC_CR_TSEL1_TIM6        /* TSEL1 = 0b0000 = TIM6_TRGO */
             | DAC_CR_DMAEN1;

    /* DMAMUX1 Ch2 (= DMA1 Ch3): request ID 6 = DAC1_CH1 */
    DMAMUX1_Channel2->CCR = DMAMUX_DAC1_CH1_ID;

    /* DMA1 Ch3: mem→periph, circular, 8-bit/8-bit, medium priority */
    DMA1_Channel3->CPAR = (uint32_t)&DAC1->DHR8R1;
    DMA1_Channel3->CCR  = DMA_CCR_DIR          /* mem→periph */
                        | DMA_CCR_CIRC         /* circular   */
                        | DMA_CCR_MINC         /* mem incr   */
                        | DMA_CCR_PSIZE_8      /* periph 8-bit */
                        | DMA_CCR_MSIZE_8      /* mem 8-bit  */
                        | DMA_CCR_PL_MED;      /* priority medium */

    /* Cache clip lengths */
    g_clip_lens_rt[0] = audio_1_len;
    g_clip_lens_rt[1] = audio_2_len;
    g_clip_lens_rt[2] = audio_3_len;

    /* Park DAC at mid-scale */
    DAC1->DHR8R1 = 0x80U;
}

/* 3 min of unheeded nagging (slouch/too-long) → "put your phone down" */
#define PHONE_ESCALATE_MS  40000UL   /* DEMO: 40s (prod = 180000UL / 3 min) */

static void audio_play_clip(uint8_t idx) {
    /* Stop any running transfer */
    TIM6->CR1            &= ~TIM6_CR1_CEN;    /* CEN=0 */
    DMA1_Channel3->CCR   &= ~DMA_CCR_EN;      /* EN=0  */
    DMA1->IFCR            = DMA_IFCR_CGIF3;   /* clear all CH3 flags */

    /* Point DMA at this clip */
    DMA1_Channel3->CMAR  = (uint32_t)g_clips[idx];
    DMA1_Channel3->CNDTR = g_clip_lens_rt[idx];

    /* Record duration */
    g_audio_clip_ms = (g_clip_lens_rt[idx] * 1000UL) / AUDIO_SAMPLE_RATE;

    /* Start */
    DMA1_Channel3->CCR |= DMA_CCR_EN;         /* EN=1  */
    TIM6->CR1          |= TIM6_CR1_CEN;       /* CEN=1 */

    g_audio_playing  = 1;
    g_audio_last_ms  = millis();
}

static void audio_stop(void) {
    TIM6->CR1          &= ~TIM6_CR1_CEN;      /* CEN=0 */
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;        /* EN=0  */
    DAC1->DHR8R1        = 0x80U;              /* mid-scale, avoids pop */
    g_audio_playing     = 0;
}

static void task_audio(void) {
    uint32_t now = millis();
    static state_t prev = S_GOOD;
    static uint32_t alert_since = 0;
    if (g_state != prev) { alert_since = now; prev = g_state; }   /* reset nag clock on change */

    /* End the clip once its duration has elapsed */
    if (g_audio_playing && (now - g_audio_last_ms) >= g_audio_clip_ms) {
        audio_stop();
    }

    /* Pick clip by problem: slouch→straighten, too-long→break, sustained→phone */
    int clip = -1;
    if (g_state == S_SLOUCH)        clip = 0;   /* "straighten up"      */
    else if (g_state == S_TOO_LONG) clip = 1;   /* break time           */
    if ((g_state == S_SLOUCH || g_state == S_TOO_LONG) &&
        (now - alert_since) > PHONE_ESCALATE_MS) clip = 2;   /* "put your phone down" */

    if (clip >= 0 && !g_audio_playing && (now - g_audio_last_ms) >= AUDIO_COOLDOWN_MS) {
        audio_play_clip((uint8_t)clip);
    }
}

int main(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;

    sched_init();
    uart_init();
    leds_init();
    btn_init();
    gpio_mode(GPIOA, 5, 1);   /* PA5 on-board LED = heartbeat */
    i2c_init();
    mpu_init();
    servo_init();
    hcsr04_init();
    audio_init();

    uart_print("Smart Desk Coach online\r\n");
    uart_print("Clip MPU to body, then press blue button to set baseline.\r\n");

    sched_add(task_button,  50,   1);
    sched_add(task_posture, 100,  1);
    sched_add(task_temp,    2000, 1);
    sched_add(task_state,   100,  1);
    sched_add(task_debug,   500,  1);
    sched_add(task_audio,   200,  1);

    sched_run();
    return 0;
}
