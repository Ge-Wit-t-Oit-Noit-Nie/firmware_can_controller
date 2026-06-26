# Controller Hierarchical Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the flat three-state controller display with a hierarchical menu: Live view → Main menu (Log / Date-Time / Send command) → Send-command submenu (Send datetime / Send advertisement req).

**Architecture:** Keep the existing enum + "each handler returns the next `CONTROLLER_STATE`" pattern, but make each handler the sole owner of its transitions (remove the top-level `switch(action)` block in the thread loop). Add a small reusable `menu_t` + `menu_render`/`menu_handle` helper so list-menu screens are data, not bespoke switch code. Convert `handle_setup_date_time` from a nested blocking loop to a single-pass handler so it matches every other screen and the live clock keeps ticking.

**Tech Stack:** C11, STM32 HAL, FreeRTOS (CMSIS-RTOS2), SSD1306 OLED driver, CMake + Ninja + arm-none-eabi-gcc.

## Global Constraints

- Edit only inside `Middlewares/gwtonn/` — never touch CubeMX-generated files (`Core/`, `Drivers/`). (CLAUDE.md)
- Code style (`.clang-format`): 4-space indent, no tabs, 80-column limit, C11.
- Must compile clean under `-Wall -Wextra -Wpedantic` — zero new warnings.
- All controller logic stays inside the existing `USER CODE BEGIN/END` context already wrapping `controller_thread_handle`.
- "Test" in this plan means: `cmake --build --preset Debug` succeeds with no new warnings. There is no host unit-test harness (bare-metal firmware). Final verification is a manual on-device button walk-through.

### Build commands (used in every task)

One-time configure (only if `build/Debug` does not exist yet):
```bash
cmake --preset Debug
```
Per-task build (the "test"):
```bash
cmake --build --preset Debug
```
Expected: ends with `[N/N] Linking ... firmware_can_controller.elf` and no `warning:`/`error:` lines.

### Files touched (whole plan)

- Modify: `Middlewares/gwtonn/src/controller.c` — all changes live here.
- No header changes required: `dt_transmit()` is already declared in `datetime.h`, reachable from `controller.c` via `#include "can.h"` (which includes `datetime.h`).

---

### Task 1: Add the reusable list-menu helper

**Files:**
- Modify: `Middlewares/gwtonn/src/controller.c`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `typedef struct { const char *title; const char *const *items; uint8_t count; uint8_t selected; } menu_t;`
  - `void menu_render(const menu_t *m);`
  - `int menu_handle(menu_t *m, uint16_t action);` — returns `-1` (nothing), `-2` (BACK), or `>=0` (item index chosen via ENTER).

- [ ] **Step 1: Add the `menu_t` type next to `CONTROLLER_STATE`**

In the `typedef Defines` section (right after the `CONTROLLER_STATE` enum, around line 41), add:

```c
typedef struct {
    const char *title;
    const char *const *items;
    uint8_t count;
    uint8_t selected;
} menu_t;
```

- [ ] **Step 2: Declare the helper prototypes**

In the `private function prototypes` section (near line 56, alongside the other prototypes), add:

```c
void menu_render(const menu_t *m);
int menu_handle(menu_t *m, uint16_t action);
```

- [ ] **Step 3: Implement `menu_render`**

Add at the end of the file (after `ssd1306_clear`):

```c
/**
 * @brief  Render a list menu: title on the top row, one item per following
 *         row, the selected item prefixed with '>'.
 * @param  m: Pointer to the menu to render.
 */
void menu_render(const menu_t *m) {
    char line[32];

    ssd1306_Fill(Black);
    ssd1306_SetCursor(1, 1);
    ssd1306_WriteString(m->title, Font_7x10, White);

    for (uint8_t i = 0; i < m->count; i++) {
        snprintf(line, sizeof(line), "%c %s",
                 (i == m->selected) ? '>' : ' ', m->items[i]);
        ssd1306_SetCursor(1, (Font_7x10.height * (i + 1)) + 2);
        ssd1306_WriteString(line, Font_7x10, White);
    }

    ssd1306_UpdateScreen();
}
```

- [ ] **Step 4: Implement `menu_handle`**

Add after `menu_render`. NOTE for the executor: the UP/DOWN body below is the
reference (wrap-around) implementation. In learning mode, before writing it,
offer the user the chance to author the ~6 lines of UP/DOWN selection logic
themselves (wrap vs. clamp is a real UX choice). If they decline, use this:

```c
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
```

- [ ] **Step 5: Build (the "test")**

Run: `cmake --build --preset Debug`
Expected: links `firmware_can_controller.elf`, no `warning:` or `error:`.
(Helpers are unused so far — GCC will not warn on unused non-static file-scope functions, so this compiles clean.)

- [ ] **Step 6: Commit**

```bash
git add Middlewares/gwtonn/src/controller.c
git commit -m "feat(controller): add reusable list-menu helper"
```

---

### Task 2: Add the new states and their two menu handlers

**Files:**
- Modify: `Middlewares/gwtonn/src/controller.c`

**Interfaces:**
- Consumes: `menu_t`, `menu_render`, `menu_handle` (Task 1); `dt_transmit()` (datetime.h, via can.h).
- Produces:
  - Enum values `STATE_MAIN_MENU`, `STATE_SEND_COMMAND`.
  - `CONTROLLER_STATE handle_main_menu(uint16_t action);`
  - `CONTROLLER_STATE handle_send_command(uint16_t action);`
  - `void show_sent_confirmation(void);`

- [ ] **Step 1: Extend the state enum**

Replace the `CONTROLLER_STATE` enum (lines 37-41) with:

```c
typedef enum ENUM_CONTROLLER_STATE {
    STATE_LIVE_VIEW = 0,
    STATE_MAIN_MENU,
    STATE_LOG_VIEW,
    STATE_SETUP_DATE_TIME,
    STATE_SEND_COMMAND,
} CONTROLLER_STATE;
```

- [ ] **Step 2: Declare the new prototypes**

In the `private function prototypes` section, add:

```c
CONTROLLER_STATE handle_main_menu(uint16_t action);
CONTROLLER_STATE handle_send_command(uint16_t action);
void show_sent_confirmation(void);
```

- [ ] **Step 3: Implement `show_sent_confirmation`**

Add near `ssd1306_clear` at the end of the file:

```c
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
```

- [ ] **Step 4: Implement `handle_main_menu`**

Add after `show_sent_confirmation`:

```c
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
```

- [ ] **Step 5: Implement `handle_send_command`**

Add after `handle_main_menu`:

```c
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
```

- [ ] **Step 6: Build (the "test")**

Run: `cmake --build --preset Debug`
Expected: clean link, no new warnings. (`handle_main_menu`/`handle_send_command` are still unreferenced — file-scope non-static, so no unused-function warning. They get wired in Task 3.)

- [ ] **Step 7: Commit**

```bash
git add Middlewares/gwtonn/src/controller.c
git commit -m "feat(controller): add main menu and send-command submenu handlers"
```

---

### Task 3: Rewire the thread loop and live/log handlers around the new states

**Files:**
- Modify: `Middlewares/gwtonn/src/controller.c`

**Interfaces:**
- Consumes: `handle_main_menu`, `handle_send_command` (Task 2); existing `handle_state_live_view`, `handle_log_view`, `handle_setup_date_time`.
- Produces: `handle_state_live_view` gains a `uint16_t action` parameter.

This task removes the top-level `switch(action)` block (which previously owned
ENTER/BACK transitions) and moves that ownership into each handler.

- [ ] **Step 1: Change the `handle_state_live_view` prototype**

In the prototypes section, change:

```c
CONTROLLER_STATE handle_state_live_view(void);
```
to:
```c
CONTROLLER_STATE handle_state_live_view(uint16_t action);
```

- [ ] **Step 2: Replace the body of `controller_thread_handle`'s loop**

Replace the entire `for (;;) { ... }` block in `controller_thread_handle`
(lines 78-164) with:

```c
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
```

(The `osDelay(10)` replaces the previous busy-spin and matches the CAN task's
polling cadence described in CLAUDE.md.)

- [ ] **Step 3: Make `handle_state_live_view` consume `action` and open the menu**

Change the signature of `handle_state_live_view` from `(void)` to
`(uint16_t action)`, and at the very top of the function body add:

```c
CONTROLLER_STATE handle_state_live_view(uint16_t action) {
    if (action == CONTROL_ACTION_ENTER) {
        ssd1306_clear();
        return STATE_MAIN_MENU;
    }

    can_message_t message;
    /* ...rest of the existing body unchanged... */
```

Leave the rest of the existing function (clock print, `can_get_last_message`,
"No messages") exactly as-is, still ending with `return STATE_LIVE_VIEW;`.

- [ ] **Step 4: Make `handle_log_view` own its BACK transition**

At the top of `handle_log_view` body, before the `switch (last_action)`, add:

```c
    if (last_action == CONTROL_ACTION_BACK) {
        ssd1306_clear();
        last_index_for_history = 0;
        return STATE_MAIN_MENU;
    }
```

Leave the rest unchanged (UP/DOWN index movement, `display_message`,
`return STATE_LOG_VIEW;`).

- [ ] **Step 5: Build (the "test")**

Run: `cmake --build --preset Debug`
Expected: clean link, no warnings. At this point Live↔Menu↔Log/Send navigation
is fully wired; only the Date/Time screen still uses its old blocking loop
(fixed in Task 4) — but it already returns to live view on its own, so the
build is functional.

- [ ] **Step 6: Commit**

```bash
git add Middlewares/gwtonn/src/controller.c
git commit -m "feat(controller): dispatch states to self-owned handlers via main menu"
```

---

### Task 4: Convert the Date/Time setup screen to single-pass

**Files:**
- Modify: `Middlewares/gwtonn/src/controller.c`

**Interfaces:**
- Consumes: existing RTC write logic inside `handle_setup_date_time`.
- Produces: `handle_setup_date_time(uint16_t action)` is now single-pass
  (no nested `for(;;)`); returns `STATE_MAIN_MENU` on BACK and on final commit.

The current function renders once, then enters its own `for(;;)` that re-reads
the queue. We remove that nested loop: the function now renders + processes at
most one `action` per call (the thread loop calls it every iteration), with
`item_selected` promoted to `static` so it survives between calls.

- [ ] **Step 1: Promote `item_selected` to static and drop the duplicate pre-render**

Replace the head of `handle_setup_date_time` (from its opening brace through the
first `ssd1306_UpdateScreen();` that precedes the `for (;;)`, i.e. the block
declaring `item_selected`, doing the first `dt_get_current_datetime`, and the
first render) with this single render-each-call head:

```c
CONTROLLER_STATE handle_setup_date_time(uint16_t action) {
    // 0=year,1=month,2=day,3=hour,4=minute,5=second
    static uint8_t item_selected = 0;

    RTC_DateTypeDef date = {0};
    RTC_TimeTypeDef time = {0};
    static datetime_t new_time = {0};
    char string_to_print[32];

    // Refresh from RTC only when the field cursor is at the start (i.e. we just
    // (re)entered this screen). Once editing has begun we keep the edited value.
    if (item_selected == 0 && action != CONTROL_ACTION_UP &&
        action != CONTROL_ACTION_DOWN) {
        dt_get_current_datetime(&new_time);
        if (new_time.year < 2000) {
            new_time.year = 2025;
        }
    }

    ssd1306_Fill(Black);
    ssd1306_SetCursor(1, 1);
    ssd1306_WriteString("Set Date/Time", Font_11x18, White);
```

- [ ] **Step 2: Remove the nested `for (;;)` wrapper and its inner re-render/queue-get**

Delete the `/* Infinite loop */ for (;;) {` line, the second copy of the
title+date+time render that immediately follows it, and the inner
`action = CONTROL_ACTION_NONE; osStatus_t result = osMessageQueueGet(...); if (result == osOK) {`
guard. The body that handled `switch (action)` (UP/DOWN/ENTER/BACK) now runs
directly, once per call. Also delete the now-unmatched closing `};` of the old
`for (;;)` and the trailing `return STATE_SETUP_DATE_TIME;` that followed it —
replace that tail with a single `return STATE_SETUP_DATE_TIME;` at the end of
the function (reached when `action` was NONE or UP/DOWN).

The `switch (action)` body keeps all existing cases unchanged EXCEPT:
  - In `CONTROL_ACTION_ENTER`'s `default:` branch (the final commit that writes
    the RTC), change `return STATE_LIVE_VIEW;` to `return STATE_MAIN_MENU;` and
    reset `item_selected = 0;` before returning.
  - In `CONTROL_ACTION_BACK`, change `return STATE_LIVE_VIEW;` to
    `return STATE_MAIN_MENU;` and reset `item_selected = 0;` before returning.

The resulting tail of the function (after the selection-underline drawing and
the final time render + `ssd1306_UpdateScreen();`) must be:

```c
    ssd1306_UpdateScreen();
    return STATE_SETUP_DATE_TIME;
}
```

- [ ] **Step 3: Confirm the two transition edits**

The ENTER final-commit branch ends with:

```c
                    ssd1306_clear();
                    item_selected = 0;
                    return STATE_MAIN_MENU;
```

The BACK branch reads:

```c
            case CONTROL_ACTION_BACK:
                ssd1306_clear();
                item_selected = 0;
                return STATE_MAIN_MENU;
```

- [ ] **Step 4: Build (the "test")**

Run: `cmake --build --preset Debug`
Expected: clean link, no warnings. Watch specifically for:
  - no "unused variable" for `result` (it was removed),
  - no "control reaches end of non-void function" (the function ends in the
    explicit `return STATE_SETUP_DATE_TIME;`).

- [ ] **Step 5: Commit**

```bash
git add Middlewares/gwtonn/src/controller.c
git commit -m "refactor(controller): make date/time setup single-pass, return to menu"
```

---

### Task 5: clang-format and on-device verification

**Files:**
- Modify: `Middlewares/gwtonn/src/controller.c` (formatting only)

- [ ] **Step 1: Format**

Run: `clang-format -i Middlewares/gwtonn/src/controller.c`

- [ ] **Step 2: Build after formatting**

Run: `cmake --build --preset Debug`
Expected: clean link, no warnings.

- [ ] **Step 3: Flash and walk the menu on hardware**

```bash
STM32_Programmer_CLI --connect port=swd --download build/Debug/firmware_can_controller.elf -hardRst -rst --start
```

Manually verify each transition (this is the real acceptance test):
  - Boot shows Live view (clock + latest message), clock keeps updating.
  - ENTER → Main Menu (Log / Date/Time / Send command), `>` marks selection.
  - UP/DOWN move the `>` selector (and wrap at the ends).
  - Main Menu → Log (ENTER on Log); UP/DOWN scroll history; BACK → Main Menu.
  - Main Menu → Date/Time (ENTER); edit fields; final ENTER writes RTC and
    returns to Main Menu; BACK from any field returns to Main Menu.
  - Main Menu → Send command (ENTER); "Send datetime" flashes "Sent!" and
    transmits; "Send advert req" flashes "Sent!" (stub); BACK → Main Menu.
  - BACK on Main Menu → Live view.

- [ ] **Step 4: Commit (if formatting changed anything)**

```bash
git add Middlewares/gwtonn/src/controller.c
git commit -m "style(controller): clang-format menu changes"
```

---

## Self-Review

**Spec coverage:**
- Start on latest message → Task 3 (live view default, ENTER opens menu). ✓
- Main menu Log / Date/Time / Send command → Task 2 (`handle_main_menu`). ✓
- Send-command submenu: Send datetime / Send advertisement req → Task 2
  (`handle_send_command`); datetime uses `dt_transmit()`, advert stubbed. ✓
- BACK up one level everywhere → Task 2 (menus return parent), Task 3 (log,
  live), Task 4 (setup). ✓
- Reusable `menu_t` + helper → Task 1. ✓
- Single-pass date/time refactor → Task 4. ✓
- Remove double-ownership top switch → Task 3. ✓
- Learning-mode `menu_handle` contribution → Task 1 Step 4 note. ✓

**Placeholder scan:** The only `// TODO` is the advertisement-request transmit,
which is intentional and matches the approved spec ("stub the send"). No vague
"add error handling" steps; all code is shown in full.

**Type consistency:** `menu_t` fields (`title`, `items`, `count`, `selected`)
used identically in Tasks 1, 2. `menu_handle` return contract (`-1`/`-2`/index)
consistent across `handle_main_menu` and `handle_send_command`. New enum values
`STATE_MAIN_MENU`/`STATE_SEND_COMMAND` used consistently in Tasks 2-4.
`handle_state_live_view(uint16_t)` signature change applied in both prototype
and definition (Task 3 Steps 1, 3).
