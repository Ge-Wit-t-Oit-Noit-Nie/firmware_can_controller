/**
 ******************************************************************************
 * @file   can.c
 * @brief  Implementation of the CAN interface
 ******************************************************************************
 *
 * The CAN Interface is used to communicate with other devices.
 * The CAN interface is implemented as an UART interface, however, for the
 * purporse of clarity, we call it CAN.
 *
 * @attention
 *
 * Copyright (c) 2025 Ge Wit't Oit Noit Nie.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
#include <string.h>

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "usart.h"

#include "can.h"
#include "controller.h"
#include "datetime.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "sd_card.h"
#include "stm32_uid.h"

#define CAN_EVENT_MESSAGE_RECEIVED 0x01
#define MAX_HISTORIC_MESSAGES 100
#define LOG_FILENAME "can_header.txt"

uint8_t uartRxBuf[UART_RX_BUF_SIZE];
uint8_t uart_rx_received;

can_message_t historic_messages[MAX_HISTORIC_MESSAGES];
uint32_t message_count = 0;

uint8_t can_store_historic_message(uint8_t *rawdata);

/**
 * @brief  FREERTOS thread handler for the CAN interface.
 *
 * @param argument: Not used.
 */
void can_thread_handler(void *argument) {

    UNUSED(argument); // Mark variable as 'UNUSED' to suppress
                      // 'unused-variable' warning

    // Initialize the event flags for CAN messages
    uart_rx_received = 0;

    // Start the UART receive process
    HAL_UART_Receive_DMA(&huart1, uartRxBuf, UART_RX_BUF_SIZE);
    for (;;) {
        // suspend the task until a message is received
        while (!uart_rx_received) {
            osDelay(10); // Sleep for 10 ms to avoid busy waiting
        }
        uart_rx_received = 0; // Reset the flag for the next message

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7,
                          GPIO_PIN_SET); // Set the LED on to indicate activity

        // store the message for later processing
        can_store_historic_message(uartRxBuf);

        HAL_GPIO_WritePin(
            GPIOB, GPIO_PIN_7,
            GPIO_PIN_RESET); // Set the LED on to indicate activity
    }
}

/**
 * @brief  Function to write data to the CAN interface.
 *
 * @param  code: Code to be sent
 * @param  data: Pointer to the data to be written.
 * @param  length: Length of the data to be written.
 */
void can_write(MESSAGE_CODE code, uint8_t *data, uint16_t length) {

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7,
                      GPIO_PIN_SET); // Set a pin to indicate activity

    uint8_t message[12] = {
        code, stm32_get_uid(), 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF}; // Initialize message buffer

    for (int i = 0; i < length && i < (UART_RX_BUF_SIZE - 5); i++) {
        message[i + 2] = data[i]; // Copy data into the message buffer
    }

    HAL_UART_Transmit(&huart1, message, 12, HAL_MAX_DELAY);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7,
                      GPIO_PIN_RESET); // Set a pin to indicate activity
}

/**
 * @brief  Callback function for UART receive complete interrupt.
 * This function is called when a byte is received via UART.
 * It stores the received byte in a circular buffer and restarts the UART
 * receive interrupt.
 *
 * Remember that this function is called from an interrupt context, so it
 * should be kept short and efficient.
 *
 * @param  huart: Pointer to the UART handle.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7,
                          GPIO_PIN_SET); // Set the LED on to indicate activity
        uart_rx_received = 1;
        HAL_GPIO_WritePin(
            GPIOB, GPIO_PIN_7,
            GPIO_PIN_RESET); // Set the LED on to indicate activity
        // Restart interrupt for next byte
        HAL_UART_Receive_DMA(&huart1, uartRxBuf, UART_RX_BUF_SIZE);
    }
}

uint8_t can_store_historic_message(uint8_t *rawdata) {

    if (rawdata == NULL) {
        return 1; // Null pointer error
    }

    if (message_count >= MAX_HISTORIC_MESSAGES) {
        message_count = 0; // Reset message count if it exceeds the maximum
    }

    datetime_t timestamp;
    dt_get_current_datetime(&timestamp); // Get the current timestamp
    // Store the message in the historic messages array
    historic_messages[message_count].timestamp = timestamp;
    historic_messages[message_count].code = rawdata[0];
    historic_messages[message_count].device_id = rawdata[1];
    memcpy(historic_messages[message_count].data, &rawdata[2],
           sizeof(historic_messages[message_count].data));
    message_count++;

    if(sd_card_is_present() == FR_OK) { 
        char string_to_store[255];
        snprintf(string_to_store, sizeof(string_to_store),
                 "[%02d:%02d:%02d],%03d,%03d,%03d,%03d,%03d,%03d,%03d,%03d,%03d\n\r",
                 timestamp.hour, timestamp.minute, timestamp.second, rawdata[0],
                 rawdata[1], rawdata[2], rawdata[3], rawdata[4], rawdata[5],
                 rawdata[6], rawdata[7], rawdata[8]);

        // Write the message to the SD card
        if (write_file(LOG_FILENAME "\0", string_to_store) != 0) {
            return 3; // Error writing to SD card
        }
    }

    return 0; // Success
}

/**
 * @brief  Function to get the last message received from the CAN interface.
 * This function retrieves the last message received from the CAN interface.
 * It fills the provided can_message_t structure with the message data.
 * @param  message: Pointer to the can_message_t structure to store the
 * message.
 * @return 0 on success, non-zero value on failure.
 */
uint8_t can_get_last_message(can_message_t *message) {

    if (message == NULL) {
        return 2; // Null pointer error
    }

    if (message_count == 0) {
        return 1; // No messages available
    }

    uint32_t index = message_count - 1; // Get the index of the last message

    // Copy the message data from the historic messages array
    message->code = historic_messages[index].code;
    message->timestamp = historic_messages[index].timestamp;
    message->device_id = historic_messages[index].device_id;
    memcpy(message->data, historic_messages[index].data, sizeof(message->data));

    return 0; // Success
}

uint32_t can_get_last_message_index(void) { return message_count-1; }

/**
 * @brief  Function to get a historic message from the CAN interface.
 *
 * This function retrieves a historic message from the CAN interface.
 * It fills the provided can_message_t structure with the message data.
 *
 * @param  index: Index of the message to retrieve.
 * @param  message: Pointer to the can_message_t structure to store the
 * message.
 * @return 0 on success, non-zero value on failure.
 */
uint8_t can_get_historic_message(uint32_t index, can_message_t *message) {

    if (index >= message_count) {
        return 1; // Index out of bounds
    }

    if (message == NULL) {
        return 2; // Null pointer error
    }

    // Copy the message data from the historic messages array
    message->code = historic_messages[index].code;
    message->device_id = historic_messages[index].device_id;
    message->timestamp = historic_messages[index].timestamp;
    memcpy(message->data, historic_messages[index].data, sizeof(message->data));

    return 0; // Success
}