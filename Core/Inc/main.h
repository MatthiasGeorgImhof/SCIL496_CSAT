/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define AVI_RST_Pin GPIO_PIN_0
#define AVI_RST_GPIO_Port GPIOC
#define SCI_RST_Pin GPIO_PIN_1
#define SCI_RST_GPIO_Port GPIOC
#define COMM1_RST_Pin GPIO_PIN_2
#define COMM1_RST_GPIO_Port GPIOC
#define COMM2_RST_Pin GPIO_PIN_3
#define COMM2_RST_GPIO_Port GPIOC
#define ATTENTION_Pin GPIO_PIN_4
#define ATTENTION_GPIO_Port GPIOA
#define EPS_RST_Pin GPIO_PIN_5
#define EPS_RST_GPIO_Port GPIOA
#define CAN2_STB_Pin GPIO_PIN_14
#define CAN2_STB_GPIO_Port GPIOB
#define CAN2_SHTD_Pin GPIO_PIN_15
#define CAN2_SHTD_GPIO_Port GPIOB
#define LED1_Pin GPIO_PIN_6
#define LED1_GPIO_Port GPIOC
#define LED2_Pin GPIO_PIN_7
#define LED2_GPIO_Port GPIOC
#define LED3_Pin GPIO_PIN_8
#define LED3_GPIO_Port GPIOC
#define LED4_Pin GPIO_PIN_9
#define LED4_GPIO_Port GPIOC
#define LED5_Pin GPIO_PIN_8
#define LED5_GPIO_Port GPIOA
#define CAN1_STB_Pin GPIO_PIN_6
#define CAN1_STB_GPIO_Port GPIOB
#define CAN1_SHTD_Pin GPIO_PIN_7
#define CAN1_SHTD_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
