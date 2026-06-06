/* Tiny cooperative scheduler with SysTick 1ms tick.
 * Exposes a FreeRTOS-flavored API: task functions run in a round-robin
 * superloop, each gated by its own next-run tick. Cooperative, not
 * preemptive — tasks must not block. Good enough for sensor polling
 * at the rates this project needs.
 */
#ifndef SCHED_H
#define SCHED_H
#include <stdint.h>
#include "stm32g474.h"

#define MAX_TASKS 8

typedef void (*task_fn)(void);

typedef struct {
    task_fn  fn;
    uint32_t period_ms;
    uint32_t next_run;
    int      enabled;
} task_t;

static volatile uint32_t g_ticks = 0;
static task_t g_tasks[MAX_TASKS];
static int g_task_count = 0;

void SysTick_Handler(void) { g_ticks++; }

static inline uint32_t millis(void) { return g_ticks; }

/* Busy-wait delay in ms (for driver init only, not inside tasks) */
static inline void delay_ms(uint32_t ms) {
    uint32_t start = g_ticks;
    while ((g_ticks - start) < ms) { __asm__("nop"); }
}

/* microsecond busy delay — assumes ~16MHz HSI core after clock setup below */
static inline void delay_us(uint32_t us) {
    /* ~16 cycles per us at 16MHz; crude but fine for HC-SR04 trigger */
    volatile uint32_t n = us * 4;
    while (n--) { __asm__("nop"); }
}

static void sched_add(task_fn fn, uint32_t period_ms, int enabled) {
    if (g_task_count >= MAX_TASKS) return;
    g_tasks[g_task_count].fn = fn;
    g_tasks[g_task_count].period_ms = period_ms;
    g_tasks[g_task_count].next_run = 0;
    g_tasks[g_task_count].enabled = enabled;
    g_task_count++;
}

/* Configure SysTick for 1ms at 16MHz HSI (default reset clock) */
static void sched_init(void) {
    SysTick->LOAD = 16000UL - 1UL;   /* 16MHz / 1000 = 1ms */
    SysTick->VAL  = 0;
    SysTick->CTRL = 0x7;             /* enable, tick int, processor clock */
}

static void sched_run(void) {
    for (;;) {
        uint32_t now = g_ticks;
        for (int i = 0; i < g_task_count; i++) {
            if (g_tasks[i].enabled && (int32_t)(now - g_tasks[i].next_run) >= 0) {
                g_tasks[i].next_run = now + g_tasks[i].period_ms;
                g_tasks[i].fn();
            }
        }
    }
}

#endif
