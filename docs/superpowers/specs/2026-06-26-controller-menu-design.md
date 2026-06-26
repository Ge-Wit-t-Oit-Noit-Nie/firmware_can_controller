# Controller Hierarchical Menu — Design

**Date:** 2026-06-26
**Component:** `Middlewares/gwtonn/src/controller.c` (+ `controller.h`)
**Status:** Approved for planning

## Goal

Replace the flat three-state display machine with a hierarchical menu. The
device starts on the latest-message live view; pressing ENTER opens a main menu
with three options (Log, Date/Time update, Send command). The Send command
option opens a submenu with two commands (Send datetime, Send advertisement
request).

## Current state (problems being fixed)

`controller_thread_handle` polls `controllerQueueHandle` non-blocking and
re-renders each iteration. Two structural issues make new screens awkward:

1. **Double-ownership of transitions.** A top-level `switch(action)` mutates
   `current_state`, and each `handle_*` ALSO returns a next state. There is no
   single place that owns navigation.
2. **`handle_setup_date_time` runs its own nested blocking `for(;;)` loop.**
   This freezes the live clock and reinvents input handling per screen.

## State model

Keep the enum + "each handler returns the next state" pattern, but **remove the
top-level `switch(action)` block** so each handler is the single owner of its
own transitions.

```
STATE_LIVE_VIEW       (default; latest message + clock)
  └─ ENTER ─→ STATE_MAIN_MENU            items: [Log, Date/Time, Send command]
                ├─ ENTER on Log ───────→ STATE_LOG_VIEW          (existing)
                ├─ ENTER on Date/Time ─→ STATE_SETUP_DATE_TIME   (existing, refactored)
                └─ ENTER on Send ──────→ STATE_SEND_COMMAND       items: [Send datetime, Send advertisement req]
```

### BACK behavior (up one level everywhere)

| From | BACK goes to |
|------|--------------|
| `STATE_MAIN_MENU` | `STATE_LIVE_VIEW` |
| `STATE_LOG_VIEW` | `STATE_MAIN_MENU` |
| `STATE_SETUP_DATE_TIME` | `STATE_MAIN_MENU` |
| `STATE_SEND_COMMAND` | `STATE_MAIN_MENU` |

The existing `handle_log_view` and `handle_setup_date_time` currently return
`STATE_LIVE_VIEW` on BACK; both are retargeted to `STATE_MAIN_MENU`.

### Enum

```c
typedef enum ENUM_CONTROLLER_STATE {
    STATE_LIVE_VIEW = 0,
    STATE_MAIN_MENU,
    STATE_LOG_VIEW,
    STATE_SETUP_DATE_TIME,
    STATE_SEND_COMMAND,
} CONTROLLER_STATE;
```

## Reusable menu component (in controller.c)

```c
typedef struct {
    const char *title;
    const char *const *items;
    uint8_t count;
    uint8_t selected;
} menu_t;

// Renders title + each item; the selected row is marked (e.g. '>' prefix).
void menu_render(const menu_t *m);

// UP/DOWN move the selection; returns:
//   -1  nothing happened (no actionable input this iteration)
//   -2  BACK pressed
//   >=0 item index chosen (ENTER)
int menu_handle(menu_t *m, uint16_t action);
```

- `STATE_MAIN_MENU` and `STATE_SEND_COMMAND` are each a `static menu_t` plus a
  small switch on the index returned by `menu_handle`.
- Selection persists across loop iterations via `static` storage.
- Adding a future menu = declare an items array + one handler.

### Selection wrap-vs-clamp (learning-mode contribution)

`menu_handle`'s UP/DOWN logic (~6 lines) is the user's to implement. The
decision is whether selection **wraps** (past last → first) or **clamps** (stops
at the ends). The function will be stubbed with a clear `TODO` and signature in
place.

## Main loop (after refactor)

```c
for (;;) {
    uint16_t action = CONTROL_ACTION_NONE;
    osMessageQueueGet(controllerQueueHandle, &action, NULL, 0); // non-blocking

    switch (current_state) {
    case STATE_LIVE_VIEW:       current_state = handle_state_live_view(action); break;
    case STATE_MAIN_MENU:       current_state = handle_main_menu(action);       break;
    case STATE_LOG_VIEW:        current_state = handle_log_view(action);        break;
    case STATE_SETUP_DATE_TIME: current_state = handle_setup_date_time(action); break;
    case STATE_SEND_COMMAND:    current_state = handle_send_command(action);    break;
    default: break;
    }
}
```

Each handler owns its own `ssd1306_Fill(Black)` / render. Live view continues to
render every iteration so the clock stays live.

## Date/Time setup refactor

Convert `handle_setup_date_time` to **single-pass**: remove the nested
`for(;;)`, promote `item_selected` to `static`, render once per call, process at
most one queued action per call. The field-editing logic (year/month/day/
hour/minute/second increment/decrement and RTC write on final ENTER) is
unchanged. BACK and the final ENTER return `STATE_MAIN_MENU` instead of
`STATE_LIVE_VIEW`. (Final ENTER may still return to live view if preferred — to
be confirmed in the plan; default: return to main menu for consistency.)

## Send command actions

- **Send datetime** → call existing `dt_transmit()` (declared `datetime.h:164`),
  show a brief "Sent!" confirmation, return to `STATE_SEND_COMMAND`.
- **Send advertisement req** → `// TODO`: a `MESSAGE_ADVERTISE_REQ` code +
  payload is not yet defined. Stub the transmit, show "Sent!", return to
  `STATE_SEND_COMMAND`.

## Constraints

- Edit only within `Middlewares/gwtonn/` (per CLAUDE.md). No CubeMX-generated
  files change.
- Must compile clean under `-Wall -Wextra -Wpedantic`, C11, 4-space indent,
  80-col (`.clang-format`).
- `controller_thread_handle` lives behind `USER CODE` guards already; its body
  is edited in place.

## Testing

Bare-metal firmware, no host test harness. Verification is:

1. `cmake --build --preset Debug` compiles clean (no new warnings).
2. Manual on-device button walk-through of every transition and BACK path.

## Out of scope

- Defining the advertisement-request wire protocol.
- Any change to `can.c`, FreeRTOS task/queue config, or the SSD1306 driver.
```
