/**
 ******************************************************************************
 * @file   stm32_uid.c
 * @brief  Maps the STM32 96-bit unique ID to an 8-bit CAN device ID.
 ******************************************************************************
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
#include "stm32f4xx_hal.h"

#include "stm32_uid.h"

/**
 * @brief  Derive a stable 8-bit device ID from the MCU's 96-bit unique ID.
 */
uint8_t stm32_get_uid(void) {
    uint32_t w0 = HAL_GetUIDw0();
    uint32_t w1 = HAL_GetUIDw1();
    uint32_t w2 = HAL_GetUIDw2();

    /* FNV-1a 32-bit over all 12 UID bytes */
    const uint8_t bytes[12] = {
        (uint8_t)(w0),       (uint8_t)(w0 >> 8),
        (uint8_t)(w0 >> 16), (uint8_t)(w0 >> 24),
        (uint8_t)(w1),       (uint8_t)(w1 >> 8),
        (uint8_t)(w1 >> 16), (uint8_t)(w1 >> 24),
        (uint8_t)(w2),       (uint8_t)(w2 >> 8),
        (uint8_t)(w2 >> 16), (uint8_t)(w2 >> 24),
    };
    uint32_t hash = 2166136261UL; /* FNV offset basis */
    for (int i = 0; i < 12; i++) {
        hash ^= bytes[i];
        hash *= 16777619UL; /* FNV prime */
    }
    /* XOR-fold 32 bits → 8 bits so all hash bits contribute */
    uint8_t id = (uint8_t)(hash ^ (hash >> 8) ^ (hash >> 16) ^ (hash >> 24));

    if (id == 0xFF) {
        id = 0xFE; /* avoid the broadcast address */
    }

    return id;
}
