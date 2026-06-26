/**
 ******************************************************************************
 * @file   can.h
 * @brief  Headerfile for the can.c file
 ******************************************************************************
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
#ifndef __MIDDLEWARES_GWTONN_CAN_H__
#define __MIDDLEWARES_GWTONN_CAN_H__

#include "datetime.h"

#ifdef __cplusplus
extern "C"
{
#endif // End of extern "C" block

#define UART_RX_BUF_SIZE 12

typedef struct {
    uint8_t code;
    uint8_t device_id;
    uint8_t data[8];
    datetime_t timestamp; // Timestamp of the message
} can_message_t;

typedef enum
{
    MESSAGE_CODE_TIME = 0x01,
    MESSAGE_LOG_EVENT = 0x02,
    MESSAGE_LOG_TELEMETRY = 0x03,
    MESSAGE_LOG_LOAD_PROGRAM = 0x04,
    MESSAGE_BOARD_STARTED = 0x05,
    MESSAGE_ADVERTISE_REQ = 0x06,
} MESSAGE_CODE;

void can_write(MESSAGE_CODE message, uint8_t *data, uint16_t length);
uint8_t can_get_historic_message(uint32_t index, can_message_t *message);
uint8_t can_get_last_message(can_message_t *message);
uint32_t can_get_last_message_index(void);

#ifdef __cplusplus
}
#endif // End of extern "C" block

#endif // __MIDDLEWARES_GWTONN_CAN_H__
