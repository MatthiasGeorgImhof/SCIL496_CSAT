#ifndef MOCK_HAL_TIMER_H
#define MOCK_HAL_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//--- Timer Channel Defines ---
#define TIM_CHANNEL_1 ((uint32_t)0x0001)
#define TIM_CHANNEL_2 ((uint32_t)0x0002)
#define TIM_CHANNEL_3 ((uint32_t)0x0004)
#define TIM_CHANNEL_4 ((uint32_t)0x0008)
#define MAX_TIMER_CHANNELS 4

//--- Timer Mock Structures ---
typedef struct {
    bool pwm_started[MAX_TIMER_CHANNELS];
    uint32_t compare_value[MAX_TIMER_CHANNELS];
    uint32_t arr_value; // Auto-reload value
} MockTimerState;

typedef struct {
    void* Instance; // Timer instance (TIM15, TIM16, etc.)
    MockTimerState State;
} TIM_HandleTypeDef;

//--- Timer Mock Function Prototypes ---
void HAL_TIM_PWM_Start(TIM_HandleTypeDef* htim, uint32_t channel);
void HAL_TIM_PWM_Stop(TIM_HandleTypeDef* htim, uint32_t channel);
void __HAL_TIM_SET_COMPARE(TIM_HandleTypeDef* htim, uint32_t channel, uint32_t value);

//--- Accessors for Testing ---
bool is_pwm_started(TIM_HandleTypeDef* htim, uint32_t channel);
uint32_t get_compare_value(TIM_HandleTypeDef* htim, uint32_t channel);
void reset_timer_state(TIM_HandleTypeDef* htim);

#ifdef __cplusplus
}
#endif

#endif // MOCK_HAL_TIMER_H
