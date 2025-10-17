/*
 * cpphal.h
 *
 *  Created on: Sep 14, 2025
 *      Author: mgi
 */

#ifndef INC_CPPHAL_H_
#define INC_CPPHAL_H_

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c4;

extern RTC_HandleTypeDef hrtc;

extern DCMI_HandleTypeDef hdcmi;

extern GPIO_TypeDef GPIOA_object;
extern GPIO_TypeDef GPIOB_object;
extern GPIO_TypeDef GPIOC_object;
extern GPIO_TypeDef GPIOD_object;
extern GPIO_TypeDef GPIOE_object;

#endif /* INC_CPPHAL_H_ */
