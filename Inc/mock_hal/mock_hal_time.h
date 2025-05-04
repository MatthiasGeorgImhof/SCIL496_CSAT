#ifndef MOCK_HAL_TIME_H
#define MOCK_HAL_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Include core definitions
#include "mock_hal/mock_hal_core.h"

//--- Time Mock Function Prototypes ---
void HAL_Delay(uint32_t Delay);
uint32_t HAL_GetTick(void);
void     HAL_SetTick(uint32_t tick);

void set_current_tick(uint32_t tick);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_HAL_TIME_H */