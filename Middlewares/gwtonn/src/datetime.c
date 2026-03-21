
/**
 * @file datetime.c
 * @brief Implementation of date and time utility functions.
 *
 * This file contains functions for handling and manipulating date and time
 * values, such as formatting, parsing, and calculating differences between
 * dates. It is designed to be used in C projects that require basic datetime
 * operations without relying on external libraries.
 *
 * @author R. Middel
 * @date 2025-06-25
 * @version 1.0
 *
 * @see datetime.h
 *
 * @copyright Copyright (c) 2025 R. Middel.
 * This file is part of the Base project and is licensed under the MIT License.
 * See the LICENSE file in the root of the project for full license text.
 * SPDX-License-Identifier: MIT
 *
 */

#include "datetime.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "rtc.h"
#include "usart.h"
#include "can.h"

/**
 * @brief Encodes a datetime structure into a dense 64-bit integer format.
 *
 */
void dt_encode(const datetime_t *datetime, dt_dense_time *encoded) {

    *encoded = (uint64_t)((int32_t)datetime->year + 0x8000);

    *encoded *= 12;
    *encoded += datetime->month;

    *encoded *= 31;
    *encoded += datetime->day;

    *encoded *= 24;
    *encoded += datetime->hour;

    *encoded *= 60;
    *encoded += datetime->minute;

    *encoded *= 60;
    *encoded += datetime->second;

    *encoded *= 1000;
    *encoded += datetime->millisecond;
}

/**
 * @brief Decodes a dense 64-bit timestamp into a datetime structure.
 *
 */
void dt_decode(dt_dense_time encoded, datetime_t *out) {

    out->millisecond = encoded % 1000;
    encoded /= 1000;

    out->second = encoded % 60;
    encoded /= 60;

    out->minute = encoded % 60;
    encoded /= 60;

    out->hour = encoded % 24;
    encoded /= 24;

    out->day = encoded % 31;
    encoded /= 31;

    out->month = encoded % 12;
    encoded /= 12;

    out->year = ((int32_t)encoded - 0x8000);
}

/**
 * @brief Prints a formatted timestamp to the specified output stream.
 *
 */
void dt_print(FILE *target, datetime_t *timestamp) {

    fprintf(target, DT_FORMAT_STRING, timestamp->year, timestamp->month,
            timestamp->day, timestamp->hour, timestamp->minute,
            timestamp->second, timestamp->millisecond);
}

/**
 * @brief Gets the current datetime as a datetime_t structure.
 * This function retrieves the current date and time from the RTC peripheral
 * and stores it in a datetime_t structure.
 * @param decoded_time Pointer to a datetime_t structure to store the current time.
 * @note The RTC peripheral must be initialized before calling this function.
 * 
 */
void dt_get_current_datetime(datetime_t *decoded_time) {
    RTC_DateTypeDef date = {0};
    RTC_TimeTypeDef time = {0};

    // Be carefull, the order of the following two calls is important
    // as HAL_RTC_GetTime() will update the time structure with the current time.
    // https://electronics.stackexchange.com/a/629777
    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    // Encode the current time
    decoded_time->year = date.Year;
    if(decoded_time->year < 70) {
        decoded_time->year += 2000;
    } else {
        decoded_time->year += 1900;
    }
    decoded_time->month = date.Month;
    decoded_time->day = date.Date;
    decoded_time->hour = time.Hours;
    decoded_time->minute = time.Minutes;
    decoded_time->second = time.Seconds;
    decoded_time->millisecond = time.SubSeconds / (1000 / time.SecondFraction);
}

/**
 * @brief Transmit the current time via CAN every minute
 * @retval None
 */
void dt_transmit()
{
    uint64_t encoded_time;
    datetime_t decoded_time = {0};
    uint8_t encoded_time_bytes[8];

    dt_get_current_datetime(&decoded_time);
    dt_encode(&decoded_time, &encoded_time);

    for (int i = 0; i < 8; i++) {
        encoded_time_bytes[i] =
            (encoded_time >> (56 - i * 8)) & 0xFF;
    }

    // Send the encoded time via CAN or other means here
    can_write(MESSAGE_CODE_TIME, encoded_time_bytes, 8);

}