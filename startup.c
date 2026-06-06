/* Startup for STM32G474RE — vector table with SysTick + TIM3 handlers */
#include <stdint.h>

extern uint32_t _sdata, _edata, _sidata, _sbss, _ebss, _estack;

int  main(void);
void SysTick_Handler(void);      /* defined in scheduler */
void EXTI15_10_Handler(void);    /* MPU INT on PA10 — optional, weak */

void Reset_Handler(void) {
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;
    main();
    while (1) { }
}

void Default_Handler(void) { while (1) { } }

/* weak aliases so unused handlers fall back to Default_Handler */
void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void EXTI15_10_Handler(void)  __attribute__((weak, alias("Default_Handler")));

/* Vector table: 16 core + first IRQs up to EXTI15_10 (position 40) */
__attribute__((section(".isr_vector")))
void (* const vectors[])(void) = {
    (void (*)(void))&_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    0, 0,
    PendSV_Handler,
    SysTick_Handler,
    /* External IRQs 0..39 — only EXTI15_10 (IRQ40) matters; pad the rest */
    [16+40] = EXTI15_10_Handler,
};
