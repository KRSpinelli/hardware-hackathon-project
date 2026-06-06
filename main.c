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
 *   Debug UART: LPUART1 PA2 TX / PA3 RX (AF12) -> ST-LINK VCP @115200
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

/* ---------- LPUART1 debug @115200 (16MHz) ----------
 * On Nucleo-G474RE the ST-LINK VCP is wired to LPUART1, not USART2.
 * PA2 = LPUART1_TX (AF12), PA3 = LPUART1_RX (AF12).
 * LPUART1 BRR = 256 * fclk / baud  (256x oversampling).
 * Clock enable is in APB1ENR2 bit 0, not APB1ENR1. */
static void uart_init(void) {
    RCC->APB1ENR2 |= (1U<<0);            /* LPUART1EN */
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    gpio_mode(GPIOA, 2, 2); gpio_af(GPIOA, 2, 12);  /* TX: PA2 AF12 */
    gpio_mode(GPIOA, 3, 2); gpio_af(GPIOA, 3, 12);  /* RX: PA3 AF12 */
    /* 256 * 16000000 / 115200 = 35556 */
    LPUART1->BRR = (256UL * 16000000UL) / 115200UL;
    LPUART1->CR1 = (1U<<3) | (1U<<2) | (1U<<0);     /* TE + RE + UE */
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
 * Audio: DAC1/TIM6/DMA1-Ch3 playback
 *
 * PA4 = DAC1_OUT1 (analog, no GPIO mode needed beyond MODER=11).
 * TIM6 fires at 8000 Hz → TRGO → DAC trigger → DMA transfer.
 * DMA1 Channel 3 (DMAMUX1 channel 2, req ID 6 = DAC1_CH1) moves
 * one byte per trigger from flash array to DAC1->DHR8R1 in circular
 * mode.  Duration-based stop: task_audio() polls millis() and calls
 * audio_stop() after the clip has played through once.
 * ============================================================ */

/* TIM6 ARR for 8 kHz at 16 MHz HSI: 16000000/8000 - 1 = 1999 */
#define AUDIO_TIM6_ARR    1999U
#define AUDIO_SAMPLE_RATE 8000UL
#define AUDIO_COOLDOWN_MS 30000UL

static const uint8_t *const g_clips[]     = { audio_1, audio_2, audio_3 };
static const uint32_t       g_clip_lens[] = { /* filled at runtime via audio_*_len */
    0, 0, 0   /* placeholder — overwritten in audio_init() */
};
/* Mutable copy of lengths (clip_lens[] above is const; we need a writable copy
 * because the extern symbols are resolved at link time, not compile time). */
static uint32_t g_clip_lens_rt[3];

static volatile uint8_t  g_audio_idx      = 0;
static volatile uint32_t g_audio_last_ms  = 0;
static volatile uint32_t g_audio_clip_ms  = 0;
static volatile int      g_audio_playing  = 0;

static void audio_init(void) {
    /* --- Clocks --- */
    RCC->AHB1ENR  |= RCC_AHB1ENR_DMA1EN;          /* DMA1 */
    RCC->APB1ENR1 |= RCC_APB1ENR1_DAC1EN           /* DAC1 */
                  |  RCC_APB1ENR1_TIM6EN;           /* TIM6 */
    /* GPIOA already enabled in main() */

    /* --- PA4: analog mode (MODER=11), no pull --- */
    GPIOA->MODER  |=  (3U << (4*2));   /* set both bits → analog */
    GPIOA->PUPDR  &= ~(3U << (4*2));   /* no pull */

    /* --- TIM6: 8 kHz update event → TRGO --- */
    TIM6->CR1  = 0;                                 /* stop, reset */
    TIM6->PSC  = 0;                                 /* no prescaler */
    TIM6->ARR  = AUDIO_TIM6_ARR;                   /* 16MHz/8kHz - 1 */
    TIM6->CR2  = TIM6_CR2_MMS_UPDATE;              /* MMS=010: update→TRGO */
    TIM6->CR1  = TIM6_CR1_ARPE;                    /* ARPE, CEN=0 (not started) */

    /* --- DAC1: trigger on TIM6_TRGO, DMA enabled --- */
    DAC1->CR = 0;                                   /* reset */
    DAC1->CR = DAC_CR_TEN1                          /* trigger enable */
             | DAC_CR_TSEL1_TIM6                    /* TSEL1=0000 → TIM6_TRGO */
             | DAC_CR_DMAEN1                        /* DMA request enable */
             | DAC_CR_EN1;                          /* channel 1 enable */

    /* Park output at mid-scale (0x80 = silence for unsigned PCM) */
    DAC1->DHR8R1 = 0x80U;

    /* --- DMAMUX1 channel 2 → DAC1_CH1 (req ID 6) --- */
    DMAMUX1_Channel2->CCR = DMAMUX_REQ_DAC1_CH1;

    /* --- DMA1 Channel 3: mem→periph, circular, 8-bit both sides --- */
    DMA1_Channel3->CCR = 0;                         /* disable first */
    DMA1_Channel3->CPAR  = (uint32_t)&DAC1->DHR8R1;
    DMA1_Channel3->CMAR  = (uint32_t)g_clips[0];
    DMA1_Channel3->CNDTR = g_clip_lens_rt[0];
    DMA1_Channel3->CCR   = DMA_CCR_DIR             /* mem→periph */
                         | DMA_CCR_CIRC            /* circular */
                         | DMA_CCR_MINC            /* memory increment */
                         | DMA_CCR_PSIZE_8         /* periph 8-bit */
                         | DMA_CCR_MSIZE_8         /* memory 8-bit */
                         | DMA_CCR_PL_MED;         /* medium priority */
    /* DMA not enabled yet — audio_play_next() enables it */

    /* Copy extern lengths into mutable runtime array */
    g_clip_lens_rt[0] = audio_1_len;
    g_clip_lens_rt[1] = audio_2_len;
    g_clip_lens_rt[2] = audio_3_len;
}

static void audio_stop(void) {
    /* Stop TIM6 first so no more DMA requests are generated */
    TIM6->CR1 &= ~TIM6_CR1_CEN;
    /* Disable DMA channel (must be done while timer is stopped) */
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;
    /* Park DAC at mid-scale to avoid a DC pop */
    DAC1->DHR8R1 = 0x80U;
    g_audio_playing = 0;
}

static void audio_play_next(void) {
    /* Stop any in-progress playback */
    TIM6->CR1 &= ~TIM6_CR1_CEN;
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;

    /* Clear any pending DMA flags for channel 3 */
    DMA1->IFCR = DMA_IFCR_CGIF3;

    /* Point DMA at the next clip */
    DMA1_Channel3->CMAR  = (uint32_t)g_clips[g_audio_idx];
    DMA1_Channel3->CNDTR = g_clip_lens_rt[g_audio_idx];

    /* Record clip duration in ms */
    g_audio_clip_ms = (g_clip_lens_rt[g_audio_idx] * 1000UL) / AUDIO_SAMPLE_RATE;

    /* Advance round-robin index */
    g_audio_idx = (g_audio_idx + 1U) % 3U;

    /* Enable DMA, then start TIM6 */
    DMA1_Channel3->CCR |= DMA_CCR_EN;
    TIM6->CR1 |= TIM6_CR1_CEN;

    g_audio_playing  = 1;
    g_audio_last_ms  = millis();
}

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

#define TOO_LONG_MS   1800000UL   /* 30 minutes */
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

/* ---------- TASK: audio playback (every 200ms) ---------- */
static void task_audio(void) {
    int bad = (g_state == S_SLOUCH || g_state == S_TOO_LONG || g_state == S_HOT);
    uint32_t now = millis();

    /* Start a new clip when: bad posture, not already playing, cooldown elapsed */
    if (bad && !g_audio_playing && (now - g_audio_last_ms) >= AUDIO_COOLDOWN_MS) {
        audio_play_next();
    }

    /* Stop after the clip has played through once */
    if (g_audio_playing && (now - g_audio_last_ms) >= g_audio_clip_ms) {
        audio_stop();
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
