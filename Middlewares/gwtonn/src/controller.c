/**
 ******************************************************************************
 * @file   controller.c
 * @brief  Implementation of the controller interface
 ******************************************************************************
 *
 * The controller interface is used to handle user input and display
 * information on the OLED display.
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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "can.h"
#include "rtc.h"

#include "cmsis_os.h"
#include "controller.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

/*****************************************************************************
 * typedef Defines
 *****************************************************************************/
typedef enum ENUM_CONTROLLER_STATE {
    STATE_LIVE_VIEW = 0,
    STATE_MAIN_MENU,
    STATE_LOG_VIEW,
    STATE_SETUP_DATE_TIME,
    STATE_SEND_COMMAND,
} CONTROLLER_STATE;

typedef struct {
    const char *title;
    const char *const *items;
    uint8_t count;
    uint8_t selected;
} menu_t;

/*****************************************************************************
 * Global Variables
 *****************************************************************************/
uint32_t last_index_for_history = 0;
extern osMessageQueueId_t controllerQueueHandle;

/*****************************************************************************
 * private function prototypes
 *****************************************************************************/
void display_message(can_message_t *message, uint8_t show_history);
CONTROLLER_STATE handle_state_live_view(uint16_t action);
CONTROLLER_STATE handle_log_view(uint16_t last_action);
CONTROLLER_STATE handle_setup_date_time(uint16_t action);

void menu_render(const menu_t *m);
int menu_handle(menu_t *m, uint16_t action);

CONTROLLER_STATE handle_main_menu(uint16_t action);
CONTROLLER_STATE handle_send_command(uint16_t action);
void show_sent_confirmation(void);

void ssd1306_clear();

/*****************************************************************************
 * Function definitions
 *****************************************************************************/
/**
 * @brief  Function to handle the controller thread.
 * @param  argument: Not used
 *
 */
void controller_thread_handle(void *argument) {
    /* USER CODE BEGIN controller_thread_handle */
    UNUSED(argument);

    CONTROLLER_STATE current_state = STATE_LIVE_VIEW;

    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    /* Infinite loop */
    for (;;) {
        uint16_t action = CONTROL_ACTION_NONE;
        osMessageQueueGet(controllerQueueHandle, &action, NULL, 0);

        switch (current_state) {
        case STATE_LIVE_VIEW:
            current_state = handle_state_live_view(action);
            break;
        case STATE_MAIN_MENU:
            current_state = handle_main_menu(action);
            break;
        case STATE_LOG_VIEW:
            current_state = handle_log_view(action);
            break;
        case STATE_SETUP_DATE_TIME:
            current_state = handle_setup_date_time(action);
            break;
        case STATE_SEND_COMMAND:
            current_state = handle_send_command(action);
            break;
        default:
            break;
        }

        osDelay(10);
    }
}

/**
 * @brief  Function to handle GPIO interrupts.
 * This function is called when a GPIO interrupt occurs. It checks the pin
 * that triggered the interrupt and sends the corresponding action to the
 * controller
 * @param  GPIO_Pin: The pin that triggered the interrupt.
 * @return None
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {

    // some debouncing could be added here if needed
    static uint32_t last_interrupt_time = 0;
    uint32_t current_time = HAL_GetTick();
    if ((current_time - last_interrupt_time) < 100) {
        return; // Ignore interrupts that occur within 100 ms of the last one
    }

    uint16_t action = 0;
    switch (GPIO_Pin) {
    case GPIO_PIN_0:
        action = CONTROL_ACTION_UP;
        break;
    case GPIO_PIN_1:
        action = CONTROL_ACTION_ENTER;
        break;
    case GPIO_PIN_2:
        action = CONTROL_ACTION_DOWN;
        break;
    case GPIO_PIN_13:
        action = CONTROL_ACTION_BACK;
        break;
    default:
        action = CONTROL_ACTION_NONE;
        break;
    }

    if (CONTROL_ACTION_NONE != action) {
        osMessageQueuePut(controllerQueueHandle, &action, 0, 0);
    }
}

/**
 * @brief  Function to display a message on the OLED display.
 * @param  message: Pointer to the can_message_t structure containing the
 * message data.
 * @param  show_history: Flag to indicate whether to use the history view.
 * @return None
 */
void display_message(can_message_t *message, uint8_t show_history) {
    // Implement your display logic here
    char string_to_print[32];
    float temp = 0;

    if (show_history) {
        snprintf(string_to_print, 32, "[Time: %02d:%02d:%02d]",
                 message->timestamp.hour, message->timestamp.minute,
                 message->timestamp.second);
        ssd1306_SetCursor(1, 1);
        ssd1306_WriteString(string_to_print, Font_7x10, White);

        snprintf(string_to_print, 32, "[Index: %03ld]", last_index_for_history);
        ssd1306_SetCursor(1, (Font_7x10.height * 4) + 1);
        ssd1306_WriteString(string_to_print, Font_7x10, White);
    }

    snprintf(string_to_print, 32, "Message from: %02d", message->device_id);
    ssd1306_SetCursor(1, Font_7x10.height + 1);
    ssd1306_WriteString(string_to_print, Font_7x10, White);

    switch (message->code) {
    case MESSAGE_CODE_TIME:
        // Do not react on this message as it is you who sends it
        break;
    case MESSAGE_LOG_EVENT:
        snprintf(string_to_print, 32, "M: %03d %03d %03d", message->data[0],
                 message->data[1], message->data[3]);
        ssd1306_SetCursor(1, (Font_7x10.height * 2) + 1);
        ssd1306_WriteString(string_to_print, Font_7x10, White);

        snprintf(string_to_print, 32, "%03d %03d %03d %03d", message->data[4],
                 message->data[5], message->data[6], message->data[7]);
        ssd1306_SetCursor(1, (Font_7x10.height * 3) + 1);
        ssd1306_WriteString(string_to_print, Font_7x10, White);

        break;
    case MESSAGE_LOG_TELEMETRY:
        // Display the temperature and VREF.
        // Note: The temperature is in degrees Celsius with two decimal
        // places.
        temp =
            (float)(((uint16_t)message->data[0] << 8) | message->data[1]) / 100;

        snprintf(string_to_print, 32, "Temp: %02d.%02d", (uint16_t)temp,
                 (uint16_t)((temp - (uint16_t)temp) * 100));
        ssd1306_SetCursor(1, (Font_7x10.height * 2) + 1);
        ssd1306_WriteString(string_to_print, Font_7x10, White);

        temp =
            (float)(((uint16_t)message->data[2] << 8) | message->data[3]) / 100;
        snprintf(string_to_print, 32, "VREF: %02d.%02d", (uint16_t)temp,
                 (uint16_t)((temp - (uint16_t)temp) * 100));
        ssd1306_SetCursor(1, (Font_7x10.height * 3) + 1);
        ssd1306_WriteString(string_to_print, Font_7x10, White);

        break;
    case MESSAGE_LOG_LOAD_PROGRAM:
        snprintf(string_to_print, 32, "LOAD PROGRAM");
        ssd1306_SetCursor(1, (Font_7x10.height * 2) + 1);
        ssd1306_WriteString(string_to_print, Font_7x10, White);
        break;

    case MESSAGE_BOARD_STARTED:
        snprintf(string_to_print, 32, "BOARD STARTED");
        ssd1306_SetCursor(1, (Font_7x10.height * 2) + 1);
        ssd1306_WriteString(string_to_print, Font_7x10, White);
        break;
        
    default:
        break;
    }

    ssd1306_UpdateScreen(); // update screen
}

/**
 * @brief  Function to handle the live view state.
 * This function retrieves the last message from the CAN interface and displays
 * it on the OLED display.
 *
 * Also prints the current time on the display.
 * @return CONTROLLER_STATE: The next state of the controller.
 */
CONTROLLER_STATE handle_state_live_view(uint16_t action) {
    if (action == CONTROL_ACTION_ENTER) {
        ssd1306_clear();
        return STATE_MAIN_MENU;
    }

    can_message_t message;

    char string_to_print[32];
    datetime_t timestamp;
    dt_get_current_datetime(&timestamp);
    snprintf(string_to_print, 32, "Time: %02d:%02d:%02d", timestamp.hour,
             timestamp.minute, timestamp.second); // get the tick count
    ssd1306_SetCursor(1, 1);
    ssd1306_WriteString(string_to_print, Font_7x10, White); // print Tick

    if (can_get_last_message(&message) == 0) {
        display_message(&message, 0);
    }
    else
    {
        ssd1306_SetCursor(1, (Font_7x10.height * 2) + 1);
        ssd1306_WriteString("No messages", Font_7x10, White);
        ssd1306_UpdateScreen(); // update screen
    }

    return STATE_LIVE_VIEW;
}

/**
 * @brief  Function to handle the log view state.
 * This function retrieves a historic message from the CAN interface and
 * displays it on the OLED display.
 * @param  last_action: The last action performed by the user.
 * @return CONTROLLER_STATE: The next state of the controller.
 */
CONTROLLER_STATE handle_log_view(uint16_t last_action) {

    if (last_action == CONTROL_ACTION_BACK) {
        ssd1306_clear();
        last_index_for_history = 0;
        return STATE_MAIN_MENU;
    }

    can_message_t message;
    switch (last_action) {
    case CONTROL_ACTION_DOWN:
        /* code */
        if (last_index_for_history > 0) {
            last_index_for_history--;
        } else {
            last_index_for_history = can_get_last_message_index();
        }
        break;
    case CONTROL_ACTION_UP:
        /* code */
        if (can_get_last_message_index() > last_index_for_history) {
            last_index_for_history++;
        } else {
            last_index_for_history = 0;
        }
        break;

    default:
        break;
    }

    if (can_get_historic_message(last_index_for_history, &message) == 0) {
        display_message(&message, 1);
    }
    return STATE_LOG_VIEW;
}

/**
 * @brief  Function to handle the setup date and time state.
 * This function allows the user to set the date and time on the device.
 * @return CONTROLLER_STATE: The next state of the controller.
 */
CONTROLLER_STATE handle_setup_date_time(uint16_t action) {

    // 0 = year, 1 = month, 2 = day, 3 = hour, 4 = minute, 5 = second
    static uint8_t item_selected = 0;
    static datetime_t new_time = {0};
    static uint8_t entered = 0;

    RTC_DateTypeDef date = {0};
    RTC_TimeTypeDef time = {0};
    char string_to_print[32];

    // Load the RTC once, the first time this screen becomes active. The
    // flag is cleared on exit (BACK/commit) so edits survive while editing.
    if (!entered) {
        dt_get_current_datetime(&new_time);
        if (new_time.year < 2000) {
            new_time.year = 2025;
        }
        entered = 1;
    }

    switch (action) {
    case CONTROL_ACTION_UP:

        switch (item_selected) {
        case 0: // Year
            new_time.year++;
            break;
        case 1: // month
            new_time.month++;
            if (new_time.month > 12) {
                new_time.month = 1;
            }
            break;
        case 2: // day
            new_time.day++;
            if (new_time.day > 31) {
                new_time.day = 1;
            }
            break;
        case 3: // Hour
            new_time.hour++;
            if (new_time.hour > 23) {
                new_time.hour = 0;
            }
            break;
        case 4: // Minute
            new_time.minute++;
            if (new_time.minute > 59) {
                new_time.minute = 0;
            }
            break;
        case 5: // second
            new_time.second++;
            if (new_time.second > 59) {
                new_time.second = 0;
            }
            break;
        default:
            break;
        }

        break;
    case CONTROL_ACTION_DOWN:

        switch (item_selected) {
        case 0: // Year
            new_time.year--;
            break;
        case 1: // month
            new_time.month--;
            if (new_time.month < 1) {
                new_time.month = 12;
            }
            break;
        case 2: // day
            new_time.day--;
            if (new_time.day < 1) {
                new_time.day = 31;
            }
            break;
        case 3: // Hour
            if (new_time.hour > 1) {
                new_time.hour--;
            } else {
                new_time.hour = 23;
            }
            break;
        case 4: // Minute
            if (new_time.minute > 1) {
                new_time.minute--;
            } else {
                new_time.minute = 59;
            }
            break;
        case 5: // second
            if (new_time.second > 1) {
                new_time.second--;
            } else {
                new_time.second = 59;
            }
            break;
        default:
            break;
        }
        break;
    case CONTROL_ACTION_ENTER:

        switch (item_selected) {
        case 0:              // Year
        case 1:              // month
        case 2:              // day
        case 3:              // Hour
        case 4:              // Minute
            item_selected++; // Move to next field
            break;

        default:

            date.Year = new_time.year - 2000; // Assuming year is 20xx
            date.Month = new_time.month;
            date.Date = new_time.day;
            date.WeekDay = 0; // Not used

            time.Hours = new_time.hour;
            time.Minutes = new_time.minute;
            time.Seconds = new_time.second;
            time.SubSeconds = 0;
            time.TimeFormat = RTC_HOURFORMAT_24;
            time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
            time.StoreOperation = RTC_STOREOPERATION_RESET;

            if (HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
                // Handle error
                Error_Handler();
            }
            if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) {
                // Handle error
                Error_Handler();
            }
            ssd1306_clear();
            item_selected = 0;
            entered = 0;
            return STATE_MAIN_MENU;
        }
        break;

    case CONTROL_ACTION_BACK:
        ssd1306_clear();
        item_selected = 0;
        entered = 0;
        return STATE_MAIN_MENU;

    default:
        break;
    }

    ssd1306_Fill(Black); // Clear the display
    ssd1306_SetCursor(1, 1);
    ssd1306_WriteString("Set Date/Time", Font_11x18, White);

    ssd1306_SetCursor(1, Font_11x18.height + 2);
    snprintf(string_to_print, 32, "%04ld-%02d-%02d", new_time.year,
             new_time.month, new_time.day);
    ssd1306_WriteString(string_to_print, Font_11x18, White);

    ssd1306_SetCursor(1, (Font_11x18.height * 2) + 2);
    snprintf(string_to_print, 32, "%02d:%02d:%02d", new_time.hour,
             new_time.minute, new_time.second);
    ssd1306_WriteString(string_to_print, Font_11x18, White);

    switch (item_selected) {
    case 0:
        ssd1306_Line(1, (Font_11x18.height * 2) + 1, Font_11x18.width * 4,
                     (Font_11x18.height * 2) + 1, White);
        break;
    case 1:
        ssd1306_Line(1 + (Font_11x18.width * 5), (Font_11x18.height * 2) + 1,
                     1 + (Font_11x18.width * 7), (Font_11x18.height * 2) + 1,
                     White);
        break;
    case 2:
        ssd1306_Line(1 + (Font_11x18.width * 8), (Font_11x18.height * 2) + 1,
                     1 + (Font_11x18.width * 10), (Font_11x18.height * 2) + 1,
                     White);
        break;
    case 3:
        ssd1306_Line(1, (Font_11x18.height * 3) + 2,
                     1 + (Font_11x18.width * 2), (Font_11x18.height * 3) + 2,
                     White);
        break;
    case 4:
        ssd1306_Line(1 + (Font_11x18.width * 3), (Font_11x18.height * 3) + 2,
                     1 + (Font_11x18.width * 5), (Font_11x18.height * 3) + 2,
                     White);
        break;
    case 5:
        ssd1306_Line(1 + (Font_11x18.width * 6), (Font_11x18.height * 3) + 2,
                     1 + (Font_11x18.width * 8), (Font_11x18.height * 3) + 2,
                     White);
        break;
    default:
        break;
    }

    ssd1306_UpdateScreen(); // update screen
    return STATE_SETUP_DATE_TIME;
}

void ssd1306_clear() {
    ssd1306_Fill(Black); // Clear the display
    ssd1306_UpdateScreen();
}

/**
 * @brief  Process one input action for a list menu.
 * @param  m: Pointer to the menu (its `selected` field may be updated).
 * @param  action: One of the CONTROL_ACTION_* values.
 * @return -1 if nothing actionable happened, -2 if BACK was pressed, or the
 *         selected item index (>= 0) if ENTER was pressed.
 */
int menu_handle(menu_t *m, uint16_t action) {
    switch (action) {
    case CONTROL_ACTION_UP:
        m->selected = (m->selected == 0) ? (m->count - 1) : (m->selected - 1);
        return -1;
    case CONTROL_ACTION_DOWN:
        m->selected = (m->selected + 1) % m->count;
        return -1;
    case CONTROL_ACTION_ENTER:
        return (int)m->selected;
    case CONTROL_ACTION_BACK:
        return -2;
    default:
        return -1;
    }
}

/**
 * @brief  Render a list menu: title on the top row, one item per following
 *         row, the selected item prefixed with '>'.
 * @param  m: Pointer to the menu to render.
 */
void menu_render(const menu_t *m) {
    char line[32];

    ssd1306_Fill(Black);
    ssd1306_SetCursor(1, 1);
    ssd1306_WriteString((char *)m->title, Font_7x10, White);

    for (uint8_t i = 0; i < m->count; i++) {
        snprintf(line, sizeof(line), "%c %s",
                 (i == m->selected) ? '>' : ' ', m->items[i]);
        ssd1306_SetCursor(1, (Font_7x10.height * (i + 1)) + 2);
        ssd1306_WriteString(line, Font_7x10, White);
    }

    ssd1306_UpdateScreen();
}

/**
 * @brief  Briefly flash a "Sent!" confirmation on the display.
 */
void show_sent_confirmation(void) {
    ssd1306_Fill(Black);
    ssd1306_SetCursor(1, (Font_11x18.height) + 2);
    ssd1306_WriteString("Sent!", Font_11x18, White);
    ssd1306_UpdateScreen();
    osDelay(800);
}

/**
 * @brief  Main menu: Log / Date-Time / Send command.
 * @param  action: One of the CONTROL_ACTION_* values.
 * @return The next controller state.
 */
CONTROLLER_STATE handle_main_menu(uint16_t action) {
    static const char *const items[] = {"Log", "Date/Time", "Send command"};
    static menu_t menu = {"Main Menu", items, 3, 0};

    switch (menu_handle(&menu, action)) {
    case 0:
        return STATE_LOG_VIEW;
    case 1:
        return STATE_SETUP_DATE_TIME;
    case 2:
        return STATE_SEND_COMMAND;
    case -2: // BACK
        return STATE_LIVE_VIEW;
    default: // -1: nothing actionable
        break;
    }

    menu_render(&menu);
    return STATE_MAIN_MENU;
}

/**
 * @brief  Send-command submenu: Send datetime / Send advertisement req.
 * @param  action: One of the CONTROL_ACTION_* values.
 * @return The next controller state.
 */
CONTROLLER_STATE handle_send_command(uint16_t action) {
    static const char *const items[] = {"Send datetime", "Send advert req"};
    static menu_t menu = {"Send Command", items, 2, 0};

    switch (menu_handle(&menu, action)) {
    case 0: // Send datetime
        dt_transmit();
        show_sent_confirmation();
        break;
    case 1: // Send advertisement request
        // TODO: define MESSAGE_ADVERTISE_REQ + payload in can.h, then
        //       can_write(MESSAGE_ADVERTISE_REQ, payload, length) here.
        show_sent_confirmation();
        break;
    case -2: // BACK
        return STATE_MAIN_MENU;
    default: // -1: nothing actionable
        break;
    }

    menu_render(&menu);
    return STATE_SEND_COMMAND;
}
