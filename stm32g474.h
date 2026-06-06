/* Minimal STM32G474RE register map — only what Smart Desk Coach needs.
 * Addresses from RM0440 (STM32G4 reference manual).
 */
#ifndef STM32G474_REGS_H
#define STM32G474_REGS_H
#include <stdint.h>

#define __IO volatile

/* ---- Bus base addresses ---- */
#define PERIPH_BASE      0x40000000UL
#define AHB1PERIPH_BASE  (PERIPH_BASE + 0x00020000UL)
#define AHB2PERIPH_BASE  0x48000000UL
#define APB1PERIPH_BASE  PERIPH_BASE
#define APB2PERIPH_BASE  (PERIPH_BASE + 0x00010000UL)

/* ---- RCC ---- */
typedef struct {
    __IO uint32_t CR; __IO uint32_t ICSCR; __IO uint32_t CFGR; __IO uint32_t PLLCFGR;
    uint32_t R0[2]; __IO uint32_t CIER; __IO uint32_t CIFR; __IO uint32_t CICR;
    uint32_t R1; __IO uint32_t AHB1RSTR; __IO uint32_t AHB2RSTR; __IO uint32_t AHB3RSTR;
    uint32_t R2; __IO uint32_t APB1RSTR1; __IO uint32_t APB1RSTR2; __IO uint32_t APB2RSTR;
    uint32_t R3; __IO uint32_t AHB1ENR; __IO uint32_t AHB2ENR; __IO uint32_t AHB3ENR;
    uint32_t R4; __IO uint32_t APB1ENR1; __IO uint32_t APB1ENR2; __IO uint32_t APB2ENR;
} RCC_TypeDef;
#define RCC ((RCC_TypeDef *)(AHB1PERIPH_BASE + 0x1000UL))

#define RCC_AHB2ENR_GPIOAEN (1U<<0)
#define RCC_AHB2ENR_GPIOBEN (1U<<1)
#define RCC_AHB2ENR_GPIOCEN (1U<<2)
#define RCC_APB1ENR1_TIM3EN (1U<<1)
#define RCC_APB1ENR1_I2C1EN (1U<<21)
#define RCC_APB2ENR_USART1EN (1U<<14)
#define RCC_APB2ENR_SYSCFGEN (1U<<0)
#define RCC_APB1ENR1_TIM2EN  (1U<<0)

/* ---- GPIO ---- */
typedef struct {
    __IO uint32_t MODER; __IO uint32_t OTYPER; __IO uint32_t OSPEEDR; __IO uint32_t PUPDR;
    __IO uint32_t IDR; __IO uint32_t ODR; __IO uint32_t BSRR; __IO uint32_t LCKR;
    __IO uint32_t AFR[2];
} GPIO_TypeDef;
#define GPIOA ((GPIO_TypeDef *)(AHB2PERIPH_BASE + 0x0000UL))
#define GPIOB ((GPIO_TypeDef *)(AHB2PERIPH_BASE + 0x0400UL))
#define GPIOC ((GPIO_TypeDef *)(AHB2PERIPH_BASE + 0x0800UL))

/* ---- I2C ---- */
typedef struct {
    __IO uint32_t CR1; __IO uint32_t CR2; __IO uint32_t OAR1; __IO uint32_t OAR2;
    __IO uint32_t TIMINGR; __IO uint32_t TIMEOUTR; __IO uint32_t ISR; __IO uint32_t ICR;
    __IO uint32_t PECR; __IO uint32_t RXDR; __IO uint32_t TXDR;
} I2C_TypeDef;
#define I2C1 ((I2C_TypeDef *)(APB1PERIPH_BASE + 0x5400UL))

/* ---- Timers (TIM2 32-bit, TIM3 16-bit) ---- */
typedef struct {
    __IO uint32_t CR1; __IO uint32_t CR2; __IO uint32_t SMCR; __IO uint32_t DIER;
    __IO uint32_t SR; __IO uint32_t EGR; __IO uint32_t CCMR1; __IO uint32_t CCMR2;
    __IO uint32_t CCER; __IO uint32_t CNT; __IO uint32_t PSC; __IO uint32_t ARR;
    __IO uint32_t RCR; __IO uint32_t CCR1; __IO uint32_t CCR2; __IO uint32_t CCR3;
    __IO uint32_t CCR4;
} TIM_TypeDef;
#define TIM2 ((TIM_TypeDef *)(APB1PERIPH_BASE + 0x0000UL))
#define TIM3 ((TIM_TypeDef *)(APB1PERIPH_BASE + 0x0400UL))

/* ---- USART1 (PA9 TX, on ST-LINK VCP path varies; we use it for debug) ---- */
typedef struct {
    __IO uint32_t CR1; __IO uint32_t CR2; __IO uint32_t CR3; __IO uint32_t BRR;
    __IO uint32_t GTPR; __IO uint32_t RTOR; __IO uint32_t RQR; __IO uint32_t ISR;
    __IO uint32_t ICR; __IO uint32_t RDR; __IO uint32_t TDR; __IO uint32_t PRESC;
} USART_TypeDef;
#define USART2 ((USART_TypeDef *)(APB1PERIPH_BASE + 0x4400UL))  /* PA2/PA3 -> ST-LINK VCP */

/* ---- SysTick (Cortex-M4 SCB) ---- */
typedef struct {
    __IO uint32_t CTRL; __IO uint32_t LOAD; __IO uint32_t VAL; __IO uint32_t CALIB;
} SysTick_TypeDef;
#define SysTick ((SysTick_TypeDef *)0xE000E010UL)

/* NVIC */
#define NVIC_ISER ((__IO uint32_t *)0xE000E100UL)
static inline void nvic_enable(int irqn) { NVIC_ISER[irqn>>5] = (1U<<(irqn&0x1F)); }

/* SYSCFG for EXTI mux */
typedef struct { __IO uint32_t MEMRMP; __IO uint32_t CFGR1; __IO uint32_t EXTICR[4]; } SYSCFG_TypeDef;
#define SYSCFG ((SYSCFG_TypeDef *)(APB2PERIPH_BASE + 0x0000UL))

/* EXTI */
typedef struct {
    __IO uint32_t IMR1; __IO uint32_t EMR1; __IO uint32_t RTSR1; __IO uint32_t FTSR1;
    __IO uint32_t SWIER1; __IO uint32_t PR1;
} EXTI_TypeDef;
#define EXTI ((EXTI_TypeDef *)(APB2PERIPH_BASE + 0x0400UL))

#endif
