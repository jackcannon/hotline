// Keyboard: a queue of timed key reports. The main loop does not wait while keys are sent.

#include "pico/stdlib.h"
#include "tusb.h"
#include "keyboard.h"

typedef struct {
    uint8_t modifier;
    uint8_t keycode;
    uint32_t ms; // How long to hold this report before the next step.
} step_t;

#define MAX_STEPS 16
static step_t steps[MAX_STEPS];
static uint32_t n_steps;
static uint32_t cur;
static bool cur_sent;
static absolute_time_t cur_end;

static void add(uint8_t modifier, uint8_t keycode, uint32_t ms) {
    if (n_steps < MAX_STEPS) {
        steps[n_steps++] = (step_t){ modifier, keycode, ms };
    }
}

void keyboard_dictation(void) {
    // macOS needs two short, separate presses.
    add(KEYBOARD_MODIFIER_LEFTCTRL, 0, 40);
    add(0, 0, 80);
    add(KEYBOARD_MODIFIER_LEFTCTRL, 0, 40);
    add(0, 0, 80);
}

void keyboard_enter(void) {
    add(0, HID_KEY_ENTER, 40);
    add(0, 0, 40);
}

void keyboard_wait(uint32_t ms) {
    add(0, 0, ms);
}

void keyboard_task(void) {
    // If the Mac does not listen, drop the keys. Old keys must not arrive later.
    if (!tud_mounted() || tud_suspended() || cur == n_steps) {
        n_steps = cur = 0;
        cur_sent = false;
        return;
    }

    if (!cur_sent) {
        if (!tud_hid_ready()) return;
        uint8_t keycodes[6] = { steps[cur].keycode };
        tud_hid_keyboard_report(0, steps[cur].modifier, keycodes);
        cur_end = make_timeout_time_ms(steps[cur].ms);
        cur_sent = true;
    } else if (time_reached(cur_end)) {
        cur++;
        cur_sent = false;
    }
}
