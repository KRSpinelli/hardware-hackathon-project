/* ============================================================
 * Smart Desk Coach — STM32 Nucleo-G474RE
 * Bare-metal drivers + cooperative scheduler (FreeRTOS-style tasks).
 *
 * Pin map (from wiring guide):
 *   I2C1:  SCL = PB8 (D15), SDA = PB9 (D14)     MPU6050@0x68, MCP9808@0x18
 *   MPU INT: PA10 (D2)
 *   HC-SR04: TRIG = PA8 (D7), ECHO = PB10 (D6, via divider)
 *   Servo:  PC7 (D9) = TIM3_CH2 PWM 50Hz
 *   LEDs:   Green PB5 (D4), Yellow PA7 (D11), Red PB3 (D3)
 *   Debug UART: USART2 PA2 (TX) -> ST-LINK VCP @115200
 *
 * NOTE: runs on default 16MHz HSI clock (no PLL) to keep timing simple.
 * ============================================================ */
#include <stdint.h>
#include "stm32g474.h"
#include "sched.h"

/* ---------- pin helpers ---------- */
static void gpio_mode(GPIO_TypeDef *p, int pin, int mode) { /* 0=in 1=out 2=af 3=analog */
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

/* ---------- USART2 debug @115200 (16MHz) ---------- */
static void uart_init(void) {
    RCC->APB1ENR1 |= (1U<<17);          /* USART2EN */
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    gpio_mode(GPIOA, 2, 2); gpio_af(GPIOA, 2, 7);  /* PA2 = AF7 USART2_TX */
    USART2->BRR = 16000000UL / 115200UL;
    USART2->CR1 = (1U<<3) | (1U<<0);    /* TE + UE */
}
static void uart_putc(char c) { while(!(USART2->ISR & (1U<<7))); USART2->TDR = c; }
static void uart_print(const char *s) { while(*s) uart_putc(*s++); }

/* ---------- I2C1 @100kHz (16MHz clock, TIMINGR from RM tables) ---------- */
static void i2c_init(void) {
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
    /* PB8 SCL, PB9 SDA: AF4, open-drain, pull-up */
    gpio_mode(GPIOB, 8, 2); gpio_af(GPIOB, 8, 4);
    gpio_mode(GPIOB, 9, 2); gpio_af(GPIOB, 9, 4);
    GPIOB->OTYPER |= (1U<<8) | (1U<<9);          /* open drain */
    GPIOB->PUPDR  &= ~((3U<<16)|(3U<<18));
    GPIOB->PUPDR  |=  (1U<<16)|(1U<<18);          /* pull-up */
    I2C1->CR1 = 0;                                /* disable to configure */
    I2C1->TIMINGR = 0x10420F13;                   /* ~100kHz @16MHz (RM0440 table) */
    I2C1->CR1 = (1U<<0);                          /* PE */
}

/* blocking write of n bytes to 7-bit addr */
static int i2c_write(uint8_t addr, const uint8_t *buf, int n) {
    I2C1->CR2 = ((uint32_t)addr<<1) | ((uint32_t)n<<16) | (1U<<13) /*START*/ | (1U<<25)/*AUTOEND*/;
    for (int i=0;i<n;i++){
        uint32_t t=200000; while(!(I2C1->ISR & (1U<<1)) && --t); /* TXIS */
        if(!t) return -1;
        I2C1->TXDR = buf[i];
    }
    uint32_t t=200000; while(!(I2C1->ISR & (1U<<5)) && --t); /* STOPF */
    I2C1->ICR = (1U<<5);
    return t?0:-1;
}
/* write register pointer then read n bytes */
static int i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, int n) {
    if (i2c_write(addr, &reg, 1) < 0) return -1;
    I2C1->CR2 = ((uint32_t)addr<<1) | ((uint32_t)n<<16) | (1U<<10)/*RD_WRN*/ | (1U<<13) | (1U<<25);
    for (int i=0;i<n;i++){
        uint32_t t=200000; while(!(I2C1->ISR & (1U<<2)) && --t); /* RXNE */
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
    uint8_t wake[2] = {0x6B, 0x00};   /* PWR_MGMT_1 = 0 -> wake */
    i2c_write(MPU_ADDR, wake, 2);
}
/* returns accel X/Y/Z in raw int16 */
static int mpu_read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t d[6];
    if (i2c_read_reg(MPU_ADDR, 0x3B, d, 6) < 0) return -1;
    *ax = (int16_t)((d[0]<<8)|d[1]);
    *ay = (int16_t)((d[2]<<8)|d[3]);
    *az = (int16_t)((d[4]<<8)|d[5]);
    return 0;
}

/* ---------- MCP9808 (0x18) ---------- */
#define MCP_ADDR 0x18
/* returns temperature in deci-degrees C (e.g. 235 = 23.5C) */
static int mcp_read_temp_dC(int *out) {
    uint8_t d[2];
    if (i2c_read_reg(MCP_ADDR, 0x05, d, 2) < 0) return -1;
    int raw = ((d[0] & 0x1F) << 8) | d[1];
    if (d[0] & 0x10) raw -= 0x2000;     /* sign */
    *out = (raw * 10) / 16;             /* 0.0625C/LSB -> deci-degrees */
    return 0;
}

/* ---------- HC-SR04: TRIG PA8, ECHO PB10. Uses TIM2 as us counter ---------- */
static void hcsr04_init(void) {
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    gpio_mode(GPIOA, 8, 1);             /* TRIG out */
    gpio_mode(GPIOB,10, 0);             /* ECHO in */
    TIM2->PSC = 16-1;                   /* 16MHz/16 = 1MHz -> 1us tick */
    TIM2->ARR = 0xFFFFFFFF;
    TIM2->CR1 = 1;
}
/* returns distance in cm, or -1 on timeout */
static int hcsr04_read_cm(void) {
    gpio_set(GPIOA, 8, 0); delay_us(3);
    gpio_set(GPIOA, 8, 1); delay_us(10);
    gpio_set(GPIOA, 8, 0);
    uint32_t guard = 300000;
    while (!gpio_read(GPIOB,10) && --guard);          /* wait rising */
    if (!guard) return -1;
    TIM2->CNT = 0;
    guard = 300000;
    while ( gpio_read(GPIOB,10) && --guard);          /* wait falling */
    if (!guard) return -1;
    uint32_t us = TIM2->CNT;
    return (int)(us / 58);                            /* us -> cm */
}

/* ---------- SG90 servo: PC7 = TIM3_CH2, 50Hz ---------- */
static void servo_init(void) {
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOCEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
    gpio_mode(GPIOC, 7, 2); gpio_af(GPIOC, 7, 2);     /* PC7 AF2 = TIM3_CH2 */
    TIM3->PSC = 16-1;            /* 1MHz */
    TIM3->ARR = 20000-1;         /* 20ms = 50Hz */
    TIM3->CCMR1 = (6U<<12)|(1U<<11);  /* CH2 PWM mode1 + preload */
    TIM3->CCER  = (1U<<4);       /* CC2E enable */
    TIM3->CCR2  = 1500;          /* center ~1.5ms */
    TIM3->CR1   = 1;
}
static void servo_us(uint16_t us) { TIM3->CCR2 = us; }   /* 1000..2000 */

/* ============================================================
 * Shared state between tasks
 * ============================================================ */
typedef enum { S_AWAY, S_GOOD, S_SLOUCH, S_TOO_LONG, S_HOT } state_t;
static volatile state_t g_state = S_AWAY;
static volatile int  g_present = 0;
static volatile int  g_slouch  = 0;
static volatile int  g_temp_dC = 0;
static volatile uint32_t g_seated_since = 0;
#define TOO_LONG_MS 30000UL      /* 30s for demo; bump for real use */
#define HOT_THRESH_dC 280        /* 28.0 C */
#define PRESENCE_CM   80         /* within 80cm = present */
#define SLOUCH_AY     8000       /* |accel Y| tilt threshold (raw) */

/* ---------- TASK: presence (HC-SR04) every 200ms ---------- */
static void task_presence(void) {
    int cm = hcsr04_read_cm();
    int now_present = (cm > 0 && cm < PRESENCE_CM);
    if (now_present && !g_present) g_seated_since = millis();
    g_present = now_present;
}

/* ---------- TASK: posture (MPU-6050) every 100ms ---------- */
static void task_posture(void) {
    int16_t ax, ay, az;
    if (mpu_read_accel(&ax,&ay,&az) == 0) {
        int tilt = ay < 0 ? -ay : ay;
        g_slouch = (tilt > SLOUCH_AY);
    }
}

/* ---------- TASK: temperature (MCP9808) every 1000ms ---------- */
static void task_temp(void) {
    int t;
    if (mcp_read_temp_dC(&t) == 0) g_temp_dC = t;
}

/* ---------- TASK: state machine + outputs every 100ms ---------- */
static void task_state(void) {
    state_t s;
    if (!g_present) {
        s = S_AWAY;
    } else if (g_temp_dC > HOT_THRESH_dC) {
        s = S_HOT;
    } else if ((millis() - g_seated_since) > TOO_LONG_MS) {
        s = S_TOO_LONG;
    } else if (g_slouch) {
        s = S_SLOUCH;
    } else {
        s = S_GOOD;
    }
    g_state = s;

    switch (s) {
        case S_AWAY:     leds_set(0,0,0); servo_us(1500); break;
        case S_GOOD:     leds_set(1,0,0); servo_us(1500); break;
        case S_SLOUCH:   leds_set(0,1,0); servo_us(1000); break;  /* arm points down */
        case S_TOO_LONG: leds_set(0,0,1);                          /* red */
                         /* wave: alternate based on tick */
                         servo_us((millis()/300)%2 ? 2000 : 1000);
                         break;
        case S_HOT:      leds_set(0,0,1);
                         uart_print("Take a movement break + cool down\r\n");
                         servo_us(1500);
                         break;
    }
}

/* ---------- TASK: heartbeat debug every 500ms ---------- */
static void task_debug(void) {
    static const char *names[] = {"AWAY","GOOD","SLOUCH","TOO_LONG","HOT"};
    GPIOA->ODR ^= (1U << 5);   /* heartbeat: toggle on-board LED PA5 */
    uart_print("state="); uart_print(names[g_state]);
    uart_print("\r\n");
}

int main(void) {
    /* clocks for GPIO ports + syscfg */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;

    sched_init();
    uart_init();
    leds_init();
    gpio_mode(GPIOA, 5, 1);   /* PA5 on-board LED = heartbeat */
    i2c_init();
    mpu_init();
    servo_init();
    hcsr04_init();

    uart_print("Smart Desk Coach online\r\n");

    /* Register tasks. To bring up one peripheral at a time, set the
     * `enabled` flag (last arg) to 0 for tasks you want to skip, flash,
     * confirm, then re-enable. All enabled here = full system. */
    sched_add(task_presence, 200,  1);
    sched_add(task_posture,  100,  1);
    sched_add(task_temp,     1000, 1);
    sched_add(task_state,    100,  1);
    sched_add(task_debug,    500,  1);

    sched_run();   /* never returns */
    return 0;
}