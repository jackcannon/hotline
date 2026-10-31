#pragma once

// RP2040 GPIO numbers. The full pin map is in CLAUDE.md.
#define PIN_MIC       29 // A0 (ADC3), ProtoMate pad 0, MAX4466 OUT
#define PIN_SPEAKER   28 // A1, ProtoMate pad 1, earpiece through the 470 ohm resistor
#define PIN_HOOK      26 // A3, ProtoMate pad 3
#define PIN_TOGGLE_A   5 // RX, toggle outer pin A
#define PIN_TOGGLE_B   3 // MO, ProtoMate pad 10, toggle outer pin B
#define PIN_LED_GREEN 24 // SDA
#define PIN_LED_RED   25 // SCL

// Level on PIN_HOOK when the handset is lifted. The input has a pull-up,
// so 0 means the hook switch closes to ground when the handset is lifted.
#define HOOK_LIFTED_LEVEL 0

// The toggle pin that reads low in the left (mic only) position.
// If left and right are the wrong way round, swap these two.
#define PIN_TOGGLE_LEFT  PIN_TOGGLE_A
#define PIN_TOGGLE_RIGHT PIN_TOGGLE_B

// A switch input must be stable for this time before a change counts.
#define DEBOUNCE_MS 30

// Auto-submit mode: the time from "stop Dictation" to Enter, so Dictation can insert the last words.
#define SUBMIT_DELAY_MS 1000

// System clock. 144 MHz divides exactly into PWM_HZ (2250 clock cycles for each period).
#define SYS_CLOCK_KHZ 144000

// PWM frequency for the LEDs and the earpiece. It is also the ADC rate: the ADC reads the mic
// once in each PWM period, from 0.2 us before the period start to 1.8 us after it (see mic.c).
// The PWM is centred (phase-correct). The earpiece pulse is centred in the middle of the period,
// so its off-time is centred on the period start, at every pulse width.
// Why:
// 1. The earpiece PWM leaks into the mic wire in the curly cord. With an ADC not in step with
//    the PWM, the leak folded down into tones at 1, 2, 3 ... 7 kHz at about -27 dBFS (a whine).
//    In step, the reading sees the same edges at the same time in every period.
// 2. The earpiece current returns through the green cord wire, which is also the mic ground.
//    Inside each period the coil current rises during the pulse and falls after it. A reading
//    just after the pulse saw the top of that sawtooth, whose height changes with the pulse
//    width in a curved way: a loop of -16 dB with a DC shift on each sound (a whine while
//    talking). In the middle of the off-time, the falling current equals its average at every
//    pulse width, so the reading sees only the average current.
// All PWM edges must stay at least 2.3 us (15 % of the period) from the period start.
#define PWM_HZ 64000

// Sidetone: the mic signal in the earpiece while the handset is lifted.
// Volume: at 100, a full-scale mic sample gives the full PWM swing.
// Limit: the earpiece signal never goes past this share of the full swing. It caps the
// loudness of feedback howl. Keep it at 40 or less, so the earpiece edges stay at least 2.3 us
// from the period start (see PWM_HZ and sidetone.c).
#define SIDETONE_VOLUME_PERCENT 50
#define SIDETONE_LIMIT_PERCENT  40

// 1: the USB stream skips the noise gate, so silence can be measured. 0: normal.
#define MIC_GATE_OFF 0

// 1: the earpiece plays a test signal (tone sweep and beeps) in place of the mic. 0: normal.
#define SIDETONE_TEST_SIGNAL 0

// LED brightness in percent (PWM duty).
#define GREEN_LED_PERCENT 8
#define RED_LED_PERCENT   80

// 1: both LEDs stay on all the time, to compare their brightness. 0: normal behaviour.
#define LED_BRIGHTNESS_TEST 0

// How long the green LED stays on.
#define GREEN_ON_MOUNT_MS 20000 // After the computer recognises the device.
#define GREEN_ON_MODE_MS  2000  // After each change of the toggle position.
