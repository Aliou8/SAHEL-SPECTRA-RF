/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32h7xx_hal.h"

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
#define RF_EN_Pin GPIO_PIN_0
#define RF_EN_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_1
#define LED1_GPIO_Port GPIOA
#define LED2_Pin GPIO_PIN_2
#define LED2_GPIO_Port GPIOA
#define LED3_Pin GPIO_PIN_3
#define LED3_GPIO_Port GPIOA
#define UHF_GPIO1_Pin GPIO_PIN_4
#define UHF_GPIO1_GPIO_Port GPIOC
#define UHF_GPIO2_Pin GPIO_PIN_5
#define UHF_GPIO2_GPIO_Port GPIOC
#define VHF_GPIO2_Pin GPIO_PIN_15
#define VHF_GPIO2_GPIO_Port GPIOE
#define VHF_GPIO1_Pin GPIO_PIN_10
#define VHF_GPIO1_GPIO_Port GPIOB
#define SPI_CS2_Pin GPIO_PIN_8
#define SPI_CS2_GPIO_Port GPIOD
#define SPI_CS3_Pin GPIO_PIN_9
#define SPI_CS3_GPIO_Port GPIOD
#define FRAM_WP_Pin GPIO_PIN_9
#define FRAM_WP_GPIO_Port GPIOA
#define CAN_SHDN_Pin GPIO_PIN_15
#define CAN_SHDN_GPIO_Port GPIOA
#define CAN_STB_Pin GPIO_PIN_2
#define CAN_STB_GPIO_Port GPIOD
#define RTC_EVI_Pin GPIO_PIN_4
#define RTC_EVI_GPIO_Port GPIOB
#define RTC_INT_Pin GPIO_PIN_5
#define RTC_INT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
