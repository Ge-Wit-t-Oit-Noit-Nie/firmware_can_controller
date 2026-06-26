/**
 ******************************************************************************
 * @file   stm32_uid.h
 * @brief  Headerfile for stm32_uid.c
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
#ifndef __MIDDLEWARES_GWTONN_STM32_UID_H__
#define __MIDDLEWARES_GWTONN_STM32_UID_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief  Derive a stable 8-bit device ID from the MCU's 96-bit unique ID.
 *
 * The returned value is in the range [0, 254]. ID 255 (0xFF) is reserved for
 * broadcast use on the CAN bus and will never be returned.
 *
 * @return Device ID in range [0, 254].
 */
uint8_t stm32_get_uid(void);

#ifdef __cplusplus
}
#endif

#endif /* __MIDDLEWARES_GWTONN_STM32_UID_H__ */
