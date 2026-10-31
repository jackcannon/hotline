#pragma once

#include <stdbool.h>
#include <stdint.h>

// Set up the ADC for the handset mic.
void mic_init(void);

// Start the ADC at a fixed time in the PWM period. Call it after the PWM starts.
void mic_start(void);

// When live is false, the mic gives silence.
void mic_set_live(bool live);

// Copy up to max samples into buf. Returns the number of samples copied.
uint32_t mic_read(int16_t *buf, uint32_t max);
