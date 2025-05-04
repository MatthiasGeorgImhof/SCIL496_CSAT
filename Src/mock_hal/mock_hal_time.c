#ifdef __x86_64__

#include "mock_hal/mock_hal_time.h"

//------------------------------------------------------------------------------
//  GLOBAL MOCKED VARIABLES - State
//------------------------------------------------------------------------------

//--- General Mock Variables ---
uint32_t current_tick = 0;

void HAL_Delay(uint32_t Delay)
{
    current_tick += Delay;
}

uint32_t HAL_GetTick(void)
{
    return current_tick;
}

void HAL_SetTick(uint32_t tick)
{
    current_tick = tick;
}

void set_current_tick(uint32_t tick){
    current_tick = tick;
}

#endif