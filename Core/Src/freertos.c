/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticQueue_t osStaticMessageQDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for controller_thre */
osThreadId_t controller_threHandle;
const osThreadAttr_t controller_thre_attributes = {
  .name = "controller_thre",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for can_thread_hand */
osThreadId_t can_thread_handHandle;
const osThreadAttr_t can_thread_hand_attributes = {
  .name = "can_thread_hand",
  .stack_size = 900 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for controllerQueue */
osMessageQueueId_t controllerQueueHandle;
uint8_t controllerQueueBuffer[ 20 * sizeof( uint32_t ) ];
osStaticMessageQDef_t controllerQueueBLock;
const osMessageQueueAttr_t controllerQueue_attributes = {
  .name = "controllerQueue",
  .cb_mem = &controllerQueueBLock,
  .cb_size = sizeof(controllerQueueBLock),
  .mq_mem = &controllerQueueBuffer,
  .mq_size = sizeof(controllerQueueBuffer)
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void controller_thread_handle(void *argument);
void can_thread_handler(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */

  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of controllerQueue */
  controllerQueueHandle = osMessageQueueNew (20, sizeof(uint32_t), &controllerQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of controller_thre */
  controller_threHandle = osThreadNew(controller_thread_handle, NULL, &controller_thre_attributes);

  /* creation of can_thread_hand */
  can_thread_handHandle = osThreadNew(can_thread_handler, NULL, &can_thread_hand_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_controller_thread_handle */
/**
  * @brief  Function implementing the controller_thre thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_controller_thread_handle */
__weak void controller_thread_handle(void *argument)
{
  /* USER CODE BEGIN controller_thread_handle */
  UNUSED(argument);

  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END controller_thread_handle */
}

/* USER CODE BEGIN Header_can_thread_handler */
/**
* @brief Function implementing the can_thread_hand thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_can_thread_handler */
__weak void can_thread_handler(void *argument)
{
  /* USER CODE BEGIN can_thread_handler */
  UNUSED(argument);

  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END can_thread_handler */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

