#pragma once

#include <stdbool.h>
#include <stdint.h>

// Set up the earpiece PWM, silent. main.c starts it.
void sidetone_init(void);

// Send one 16-bit sample to the earpiece. Call it once for each USB audio sample.
// When live is false, the earpiece fades to silence.
void sidetone_write(int32_t sample, bool live);
