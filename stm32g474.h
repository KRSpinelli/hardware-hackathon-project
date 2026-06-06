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

/* ===========================================================================
 * DAC1 — verified against stm32g474xx.h CMSIS header (RM0440 Rev 5)
 *
 * Bus:      AHB2 (not APB1 — DAC1/3 are on AHB2 in STM32G4)
 * Base:     0x50000800  (confirmed by live debug: DAC1->DHR8R1 @ 0x50000808)
 *
 * Register offsets (DAC_TypeDef from CMSIS header):
 *   CR       +0x00   Control register
 *   SWTRIGR  +0x04   Software trigger register
 *   DHR12R1  +0x08   CH1 12-bit right-aligned data holding register
 *   DHR12L1  +0x0C   CH1 12-bit left-aligned data holding register
 *   DHR8R1   +0x10   CH1  8-bit right-aligned data holding register
 *   DHR12R2  +0x14   CH2 12-bit right-aligned data holding register
 *   DHR12L2  +0x18   CH2 12-bit left-aligned data holding register
 *   DHR8R2   +0x1C   CH2  8-bit right-aligned data holding register
 *   DHR12RD  +0x20   Dual 12-bit right-aligned data holding register
 *   DHR12LD  +0x24   Dual 12-bit left-aligned data holding register
 *   DHR8RD   +0x28   Dual  8-bit right-aligned data holding register
 *   DOR1     +0x2C   CH1 data output register (read-only)
 *   DOR2     +0x30   CH2 data output register (read-only)
 *   SR       +0x34   Status register
 *   CCR      +0x38   Calibration control register
 *   MCR      +0x3C   Mode control register
 *   SHSR1    +0x40   Sample-and-hold sample time register 1
 *   SHSR2    +0x44   Sample-and-hold sample time register 2
 *   SHHR     +0x48   Sample-and-hold hold time register
 *   SHRR     +0x4C   Sample-and-hold refresh time register
 * ===========================================================================
 */
typedef struct {
    __IO uint32_t CR;       /* +0x00 Control                                  */
    __IO uint32_t SWTRIGR;  /* +0x04 Software trigger                         */
    __IO uint32_t DHR12R1;  /* +0x08 CH1 12-bit right-aligned data holding    */
    __IO uint32_t DHR12L1;  /* +0x0C CH1 12-bit left-aligned data holding     */
    __IO uint32_t DHR8R1;   /* +0x10 CH1  8-bit right-aligned data holding    */
    __IO uint32_t DHR12R2;  /* +0x14 CH2 12-bit right-aligned data holding    */
    __IO uint32_t DHR12L2;  /* +0x18 CH2 12-bit left-aligned data holding     */
    __IO uint32_t DHR8R2;   /* +0x1C CH2  8-bit right-aligned data holding    */
    __IO uint32_t DHR12RD;  /* +0x20 Dual 12-bit right-aligned data holding   */
    __IO uint32_t DHR12LD;  /* +0x24 Dual 12-bit left-aligned data holding    */
    __IO uint32_t DHR8RD;   /* +0x28 Dual  8-bit right-aligned data holding   */
    __IO uint32_t DOR1;     /* +0x2C CH1 data output (read-only)              */
    __IO uint32_t DOR2;     /* +0x30 CH2 data output (read-only)              */
    __IO uint32_t SR;       /* +0x34 Status                                   */
    __IO uint32_t CCR;      /* +0x38 Calibration control                      */
    __IO uint32_t MCR;      /* +0x3C Mode control                             */
    __IO uint32_t SHSR1;    /* +0x40 Sample-and-hold sample time 1            */
    __IO uint32_t SHSR2;    /* +0x44 Sample-and-hold sample time 2            */
    __IO uint32_t SHHR;     /* +0x48 Sample-and-hold hold time                */
    __IO uint32_t SHRR;     /* +0x4C Sample-and-hold refresh time             */
} DAC_TypeDef;

/* DAC1 base = 0x50000800 (AHB2 peripheral, NOT APB1).
 * Note: AHB2PERIPH_BASE in this file is 0x48000000 (GPIO bank).
 * DAC1/3 sit on a separate AHB2 sub-region starting at 0x50000000. */
#define DAC1_BASE  0x50000800UL
#define DAC1       ((DAC_TypeDef *)DAC1_BASE)

/* DAC CR register bit definitions (channel 1) */
#define DAC_CR_EN1      (1U << 0)   /* CH1 enable                             */
#define DAC_CR_TEN1     (1U << 2)   /* CH1 trigger enable                     */
/* TSEL1[3:0] = bits [19:16].  0b0000 = TIM6_TRGO (reset default).
 * Write the 4-bit field; do not OR individual bits to avoid stale values. */
#define DAC_CR_TSEL1_Pos  16U
#define DAC_CR_TSEL1_Msk  (0xFU << DAC_CR_TSEL1_Pos)
#define DAC_CR_TSEL1_TIM6 (0x0U << DAC_CR_TSEL1_Pos)  /* TIM6_TRGO           */
#define DAC_CR_DMAEN1   (1U << 12)  /* CH1 DMA enable                         */
#define DAC_CR_DMAUDRIE1 (1U << 13) /* CH1 DMA underrun interrupt enable      */

/* DAC SR register bit definitions */
#define DAC_SR_DMAUDR1  (1U << 13)  /* CH1 DMA underrun flag (write 1 to clr) */

/* RCC clock enables for DAC1 */
#define RCC_APB1ENR1_DAC1EN (1U << 29)  /* DAC1 clock enable (APB1ENR1 bit 29) */

/* ===========================================================================
 * TIM6 — basic timer used as DAC trigger source via TRGO
 *
 * Bus:  APB1  Base: 0x40001000  (RM0440 Table 1)
 * TIM6 has only CR1, CR2, DIER, SR, EGR, CNT, PSC, ARR — no capture/compare.
 * ===========================================================================
 */
typedef struct {
    __IO uint32_t CR1;   /* +0x00 Control register 1                          */
    __IO uint32_t CR2;   /* +0x04 Control register 2 (MMS field for TRGO)     */
    uint32_t      RSVD0; /* +0x08 (no SMCR on basic timers)                   */
    __IO uint32_t DIER;  /* +0x0C DMA/interrupt enable register               */
    __IO uint32_t SR;    /* +0x10 Status register                             */
    __IO uint32_t EGR;   /* +0x14 Event generation register                   */
    uint32_t      RSVD1[3]; /* +0x18–0x20 (no CCMR/CCER on basic timers)     */
    __IO uint32_t CNT;   /* +0x24 Counter                                     */
    __IO uint32_t PSC;   /* +0x28 Prescaler                                   */
    __IO uint32_t ARR;   /* +0x2C Auto-reload register                        */
} TIM6_TypeDef;

#define TIM6 ((TIM6_TypeDef *)(APB1PERIPH_BASE + 0x1000UL))

/* TIM6 CR1 bits */
#define TIM6_CR1_CEN    (1U << 0)   /* Counter enable                         */
#define TIM6_CR1_ARPE   (1U << 7)   /* Auto-reload preload enable             */

/* TIM6 CR2 — MMS[2:0] = bits [6:4], selects TRGO source.
 * 0b010 = Update event → TRGO (triggers DAC on each timer overflow).        */
#define TIM6_CR2_MMS_Pos  4U
#define TIM6_CR2_MMS_UPDATE (0x2U << TIM6_CR2_MMS_Pos)  /* Update → TRGO    */

/* TIM6 DIER bits */
#define TIM6_DIER_UIE   (1U << 0)   /* Update interrupt enable                */
#define TIM6_DIER_UDE   (1U << 8)   /* Update DMA request enable              */

/* TIM6 SR bits */
#define TIM6_SR_UIF     (1U << 0)   /* Update interrupt flag                  */

/* TIM6 EGR bits */
#define TIM6_EGR_UG     (1U << 0)   /* Update generation (force reload)       */

/* RCC clock enable for TIM6 */
#define RCC_APB1ENR1_TIM6EN (1U << 4)  /* TIM6 clock enable (APB1ENR1 bit 4) */

/* IRQ number — TIM6 and DAC1/3 underrun share one vector (IRQn = 54) */
#define TIM6_DAC_IRQn   54

/* ===========================================================================
 * DMA1 — used to feed DAC1_CH1 from a sample buffer in circular mode
 *
 * Bus:  AHB1  Base: 0x40020000  (RM0440 Table 1)
 * Each channel has its own CCR/CNDTR/CPAR/CMAR registers at +0x08*n offset.
 * DMA1 Channel 3 is the recommended channel for DAC1_CH1 (DMAMUX req ID 6).
 * ===========================================================================
 */
typedef struct {
    __IO uint32_t CCR;    /* +0x00 Channel configuration register             */
    __IO uint32_t CNDTR;  /* +0x04 Channel number of data register            */
    __IO uint32_t CPAR;   /* +0x08 Channel peripheral address register        */
    __IO uint32_t CMAR;   /* +0x0C Channel memory address register            */
    uint32_t      RSVD;   /* +0x10 Reserved                                   */
} DMA_Channel_TypeDef;

typedef struct {
    __IO uint32_t ISR;    /* +0x00 Interrupt status register                  */
    __IO uint32_t IFCR;   /* +0x04 Interrupt flag clear register              */
} DMA_TypeDef;

#define DMA1_BASE         (AHB1PERIPH_BASE + 0x0000UL)  /* 0x40020000        */
#define DMA1              ((DMA_TypeDef *)DMA1_BASE)

/* DMA1 channel base addresses: channel N is at DMA1_BASE + 0x08 + 0x14*(N-1) */
#define DMA1_CH1_BASE     (DMA1_BASE + 0x0008UL)
#define DMA1_CH2_BASE     (DMA1_BASE + 0x001CUL)
#define DMA1_CH3_BASE     (DMA1_BASE + 0x0030UL)
#define DMA1_CH4_BASE     (DMA1_BASE + 0x0044UL)
#define DMA1_CH5_BASE     (DMA1_BASE + 0x0058UL)
#define DMA1_CH6_BASE     (DMA1_BASE + 0x006CUL)
#define DMA1_CH7_BASE     (DMA1_BASE + 0x0080UL)
#define DMA1_CH8_BASE     (DMA1_BASE + 0x0094UL)

#define DMA1_Channel1     ((DMA_Channel_TypeDef *)DMA1_CH1_BASE)
#define DMA1_Channel2     ((DMA_Channel_TypeDef *)DMA1_CH2_BASE)
#define DMA1_Channel3     ((DMA_Channel_TypeDef *)DMA1_CH3_BASE)  /* DAC1_CH1 */
#define DMA1_Channel4     ((DMA_Channel_TypeDef *)DMA1_CH4_BASE)
#define DMA1_Channel5     ((DMA_Channel_TypeDef *)DMA1_CH5_BASE)
#define DMA1_Channel6     ((DMA_Channel_TypeDef *)DMA1_CH6_BASE)
#define DMA1_Channel7     ((DMA_Channel_TypeDef *)DMA1_CH7_BASE)
#define DMA1_Channel8     ((DMA_Channel_TypeDef *)DMA1_CH8_BASE)

/* DMA CCR register bits */
#define DMA_CCR_EN        (1U << 0)   /* Channel enable                       */
#define DMA_CCR_TCIE      (1U << 1)   /* Transfer complete interrupt enable   */
#define DMA_CCR_HTIE      (1U << 2)   /* Half-transfer interrupt enable       */
#define DMA_CCR_TEIE      (1U << 3)   /* Transfer error interrupt enable      */
#define DMA_CCR_DIR       (1U << 4)   /* Data direction: 0=P→M, 1=M→P        */
#define DMA_CCR_CIRC      (1U << 5)   /* Circular mode                        */
#define DMA_CCR_PINC      (1U << 6)   /* Peripheral address increment         */
#define DMA_CCR_MINC      (1U << 7)   /* Memory address increment             */
/* PSIZE[1:0] = bits [9:8]: 00=8-bit, 01=16-bit, 10=32-bit */
#define DMA_CCR_PSIZE_8   (0U << 8)
#define DMA_CCR_PSIZE_16  (1U << 8)
#define DMA_CCR_PSIZE_32  (2U << 8)
/* MSIZE[1:0] = bits [11:10]: 00=8-bit, 01=16-bit, 10=32-bit */
#define DMA_CCR_MSIZE_8   (0U << 10)
#define DMA_CCR_MSIZE_16  (1U << 10)
#define DMA_CCR_MSIZE_32  (2U << 10)
/* PL[1:0] = bits [13:12]: priority level */
#define DMA_CCR_PL_LOW    (0U << 12)
#define DMA_CCR_PL_MED    (1U << 12)
#define DMA_CCR_PL_HIGH   (2U << 12)
#define DMA_CCR_PL_VHIGH  (3U << 12)

/* DMA ISR / IFCR flag positions for channel N (1-indexed):
 * Each channel occupies 4 bits: GIF, TCIF, HTIF, TEIF at bit (4*(N-1)).    */
#define DMA_ISR_TCIF3     (1U << 9)   /* CH3 transfer complete flag           */
#define DMA_ISR_HTIF3     (1U << 10)  /* CH3 half-transfer flag               */
#define DMA_ISR_TEIF3     (1U << 11)  /* CH3 transfer error flag              */
#define DMA_IFCR_CGIF3    (1U << 8)   /* CH3 clear all flags                  */
#define DMA_IFCR_CTCIF3   (1U << 9)   /* CH3 clear transfer complete          */
#define DMA_IFCR_CHTIF3   (1U << 10)  /* CH3 clear half-transfer              */
#define DMA_IFCR_CTEIF3   (1U << 11)  /* CH3 clear transfer error             */

/* IRQ number for DMA1 Channel 3 */
#define DMA1_Channel3_IRQn  13

/* RCC clock enable for DMA1 */
#define RCC_AHB1ENR_DMA1EN  (1U << 0)  /* DMA1 clock enable (AHB1ENR bit 0)  */

/* ===========================================================================
 * DMAMUX1 — routes DMA request sources to DMA channels
 *
 * Bus:  AHB1  Base: 0x40020800  (immediately after DMA1 register block)
 * Each channel has one 32-bit CxCR register.
 * DMAMUX1 channels 0–7 map to DMA1 channels 1–8 (0-indexed here).
 * DMAMUX1 channels 8–15 map to DMA2 channels 1–8.
 *
 * To route DAC1_CH1 (request ID 6) to DMA1_Channel3:
 *   DMAMUX1_Channel2->CCR = 6;   (channel index 2 = DMA1 channel 3)
 * ===========================================================================
 */
typedef struct {
    __IO uint32_t CCR;  /* Channel x configuration register (DMAMUX_CxCR)    */
} DMAMUX_Channel_TypeDef;

#define DMAMUX1_BASE          (AHB1PERIPH_BASE + 0x0800UL)  /* 0x40020800    */
#define DMAMUX1_Channel0      ((DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + 0x00UL))
#define DMAMUX1_Channel1      ((DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + 0x04UL))
#define DMAMUX1_Channel2      ((DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + 0x08UL))
#define DMAMUX1_Channel3      ((DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + 0x0CUL))
#define DMAMUX1_Channel4      ((DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + 0x10UL))
#define DMAMUX1_Channel5      ((DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + 0x14UL))
#define DMAMUX1_Channel6      ((DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + 0x18UL))
#define DMAMUX1_Channel7      ((DMAMUX_Channel_TypeDef *)(DMAMUX1_BASE + 0x1CUL))

/* DMAMUX CxCR register: bits [6:0] = DMAREQ_ID (request source).
 * All other bits default to 0 (no synchronisation, no event generation).    */
#define DMAMUX_CxCR_DMAREQ_ID_Pos  0U
#define DMAMUX_CxCR_DMAREQ_ID_Msk  (0x7FU << DMAMUX_CxCR_DMAREQ_ID_Pos)

/* DMAMUX1 request IDs — from stm32g4xx_ll_dmamux.h (RM0440 Table 80)       */
#define DMAMUX_REQ_DAC1_CH1   6U   /* DAC1 channel 1 DMA request             */
#define DMAMUX_REQ_DAC1_CH2   7U   /* DAC1 channel 2 DMA request             */
#define DMAMUX_REQ_TIM6_UP    8U   /* TIM6 update DMA request                */

#endif
