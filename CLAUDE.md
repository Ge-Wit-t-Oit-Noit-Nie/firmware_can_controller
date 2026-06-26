# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Firmware for an STM32F412RGTx (Cortex-M4) CAN controller node that:
- Acts as a central hub on a "CAN" bus (physically implemented over UART)
- Stores all received messages on an SD card via FATFS
- Synchronizes time across the bus using the onboard RTC
- Provides a user interface via an SSD1306 OLED display and GPIO push-buttons

## Build Commands

This project uses CMake with Ninja and STM32CubeMX-generated presets. All commands require `arm-none-eabi-gcc` from STM32CubeCLT to be on `PATH`.

```sh
# Configure (choose preset: Debug, Release, RelWithDebInfo, MinSizeRel)
cmake --preset Debug

# Build
cmake --build --preset Debug

# Clean rebuild
cmake --build --preset Debug --clean-first
```

The built `.elf` is placed in `build/<preset>/firmware_can_controller.elf`.

## Flash / Debug

Flashing uses `STM32_Programmer_CLI` via SWD:

```sh
STM32_Programmer_CLI --connect port=swd --download build/Debug/firmware_can_controller.elf -hardRst -rst --start
```

Debugging uses `cortex-debug` (VS Code) with ST-Link GDB server — see `.vscode/launch.json`. The SVD file for peripheral register inspection is `STM32F412.svd` (path resolved from `STM32VSCodeExtension.cubeCLT.path`).

## Code Style

Enforced by `.clang-format`:
- 4-space indent, no tabs
- 80-character column limit
- C11 (`-std=c11`)
- Compiler flags: `-Wall -Wextra -Wpedantic`

Run clang-format before committing:
```sh
clang-format -i Middlewares/gwtonn/**/*.{c,h}
```

STM32CubeMX-generated files in `Core/` and `Drivers/` should not be manually reformatted.

## Architecture

### Threading model

Two FreeRTOS tasks (CMSIS-RTOS2 API) run concurrently:

| Task | Priority | Stack | Role |
|------|----------|-------|------|
| `can_thread_handler` | AboveNormal | 3600 B | Receives UART frames, stores to ring buffer + SD card |
| `controller_thread_handle` | Normal | 512 B | Handles button input, drives OLED state machine |

Inter-task communication uses a single static `osMessageQueue`: `controllerQueueHandle` (capacity 20 × `uint32_t`). GPIO EXTI callbacks post `CONTROL_ACTION_*` values into this queue; the controller task consumes them.

### "CAN" bus wire protocol

The CAN bus is **physically carried over UART** (`huart1`, DMA, 12 bytes/frame):

```
[0]  MESSAGE_CODE   (uint8)
[1]  device_id      (uint8)
[2..9] data         (8 bytes)
[10..11] 0xFF 0xFF  (trailer)
```

`HAL_UART_RxCpltCallback` sets `uart_rx_received = 1`; the CAN task polls this flag with `osDelay(10)` to avoid busy-wait.

### Message history

`can.c` maintains a ring buffer of up to 100 `can_message_t` structs in RAM (`historic_messages[]`). The index wraps at 100. Every received message is also appended to `can_header.txt` on the SD card (if a card is present).

### Controller state machine

`controller.c` implements three display states:

```
STATE_LIVE_VIEW  <──BACK──  STATE_LOG_VIEW
      │                          ▲
    ENTER                   UP/DOWN
      │                          │
      └──────► STATE_SETUP_DATE_TIME ──ENTER(final)──► STATE_LIVE_VIEW
```

### Directory structure

```
Core/               STM32CubeMX-generated HAL init and FreeRTOS config
Drivers/            STM32 HAL + CMSIS (do not edit)
FATFS/              FatFs middleware (App + SPI diskio target)
Middlewares/
  gwtonn/           Project-specific middleware (edit here)
    include/        can.h, controller.h, datetime.h, sd_card.h
    src/            Implementations
  ssd1306/          Third-party SSD1306 OLED driver
cmake/              Toolchain files (gcc-arm-none-eabi.cmake)
```

### Key source files to know

- `Middlewares/gwtonn/src/can.c` — UART reception, message storage, SD card logging
- `Middlewares/gwtonn/src/controller.c` — OLED UI state machine, GPIO ISR callback
- `Middlewares/gwtonn/src/datetime.c` — RTC abstraction (`dt_get_current_datetime`)
- `Middlewares/gwtonn/src/sd_card.c` — FATFS mount/write helpers
- `Core/Src/freertos.c` — Task and queue creation (STM32CubeMX-managed, add new tasks here inside `USER CODE` guards)
- `can_controller.ioc` — STM32CubeMX project file; re-run CubeMX here to regenerate `Core/` and `Drivers/`

## Important Constraints

- **Do not edit outside `USER CODE BEGIN/END` guards** in CubeMX-generated files (`Core/Src/*.c`, `Core/Inc/*.h`). CubeMX will overwrite those files.
- New application logic belongs in `Middlewares/gwtonn/`.
- The `can_thread_handler` stack is intentionally large (900×4 B) due to SD card and FATFS operations — do not reduce it without profiling.
- `message_count` wraps at `MAX_HISTORIC_MESSAGES` (100) — oldest messages are silently overwritten. Log files on the SD card are the permanent record.
