#ifdef __x86_64__

#include "mock_hal/mock_hal_core.h"
#include "mock_hal/mock_hal_systick.h"
#include <stdio.h>

//Globally defined mock variables for utilities, can be defined in a shared memory file
uint32_t gSysTickLoad = 0;

HAL_StatusTypeDef HAL_SYSTICK_Config(uint32_t ticks)
{
  //Mock implementation, record number of ticks in reload register
  SysTick->LOAD = ticks;
  gSysTickLoad = ticks;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_InitTick (uint32_t TickPriority)
{
  //Mock implementation
  (void)TickPriority; //Not needed
  return HAL_OK;
}

#endif /*__x86_64__*/