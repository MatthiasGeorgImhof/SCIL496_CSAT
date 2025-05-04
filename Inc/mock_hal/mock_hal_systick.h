#ifndef MOCK_HAL_SYSTICK_H
#define MOCK_HAL_SYSTICK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Include core definitions
#include "mock_hal_core.h"

//SysTick register structure (example)
typedef struct
{
  __IO uint32_t CTRL;                /*!< Offset: 0x000 (R/W)  SysTick Control and Status Register */
  __IO uint32_t LOAD;                /*!< Offset: 0x04  (R/W)  SysTick Reload Value Register       */
  __IO uint32_t VAL;                 /*!< Offset: 0x08  (R/W)  SysTick Current Value Register      */
  __IO uint32_t CALIB;               /*!< Offset: 0x0C  (R/ )  SysTick Calibration Register          */
} SysTick_Type;

#define SysTick ((SysTick_Type *)0xE000E010U)   /*!< SysTick configuration struct          */

// Declare mocks here
HAL_StatusTypeDef HAL_SYSTICK_Config(uint32_t ticks);
HAL_StatusTypeDef HAL_InitTick (uint32_t TickPriority);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_HAL_SYSTICK_H */