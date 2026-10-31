#pragma once

#include <stdint.h>

// Each function adds keys to a queue. keyboard_task() sends them in order.

// Press Control twice: the macOS Dictation shortcut, to start or stop Dictation.
void keyboard_dictation(void);

// Press Enter.
void keyboard_enter(void);

// Send no keys for this time.
void keyboard_wait(uint32_t ms);

// Send the queued keys. Call it often from the main loop.
void keyboard_task(void);
