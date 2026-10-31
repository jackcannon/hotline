// Handset mic: the ADC interrupt reads the ADC, filters the samples, and puts them in a ring buffer.

#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "mic.h"
#include "sidetone.h"
#include "config.h"
#include "tusb_config.h"

#define RING_SIZE 256 // Must be a power of 2.

static volatile int16_t ring[RING_SIZE];
static volatile uint32_t head; // Written only by the interrupt.
static volatile uint32_t tail; // Written only by mic_read.
static volatile bool is_live;

// The ADC reads OVERSAMPLE times for each USB sample, once in each PWM period (see PWM_HZ).
// Each sample is the average of OVERSAMPLE readings, which reduces the ADC noise (hiss) by about 6 dB.
#define OVERSAMPLE 4
_Static_assert(PWM_HZ == CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE * OVERSAMPLE, "PWM_HZ must be the ADC rate");
static uint32_t os_sum;
static uint32_t os_count;

// The ADC runs by itself at PWM_HZ from its 48 MHz clock, and the PWM runs at PWM_HZ from the
// system clock. Both clocks come from the 12 MHz crystal, so they stay locked together. mic_start()
// starts the ADC once, so every reading is at the same time in the PWM period: it starts 0.2 us
// before the period start and takes 2 us (see PWM_HZ for why).
// (Readings started from an interrupt were sometimes late, which put clicks on the mic.)
#define ADC_PHASE_CYCLES (SYS_CLOCK_KHZ * 1000 / PWM_HZ - SYS_CLOCK_KHZ * 2 / 10000) // one period less 0.2 us

// DC filter: it removes the DC offset of the MAX4466 output (cutoff about 2.5 Hz).
// Values are ADC counts with 8 fractional bits. It starts at mid-scale (VCC/2).
static int32_t dc = 2048 << 8;

// Buzz filter: it learns the pattern of the signal that repeats every 1 ms (the buzz from USB
// activity at 1, 2, 3 ... 8 kHz) and subtracts it. It learns only while the level is below
// BUZZ_LEARN_LEVEL: the buzz is always below it, and speech is above it. So speech does not get
// into the pattern, and the buzz cannot stop the learning.
// (A comb notch filter that learns all the time left a fading echo at 1, 2, 3 ... 8 kHz for
// about 0.13 s after each sudden end of sound: a metallic ping in the earpiece. A version that
// learned only while the noise gate was closed, with no DC filter, locked up: the DC offset kept
// the gate open, so it never learned.)
// While it learns, it is the same as a comb notch filter with notches about 2.5 Hz wide.
#define BUZZ_LEARN_LEVEL 300 // In 16-bit sample units, about -41 dBFS. The buzz peaks at about 100.
#define PATTERN_LEN (CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE / 1000)
static int32_t pattern[PATTERN_LEN];
static uint32_t pattern_idx;

// Noise gates: they mute the hiss between words. There are two, with different settings.
// USB gate: it opens at a low level, so Dictation gets the soft start of each word. Its level
// detector sees each sample 5 ms before the output does, so the gate opens before a word starts.
// Earpiece gate: it opens at a higher level and closes sooner, so breath and taps do not switch
// the hiss on in the earpiece. It has no look-ahead, so the sidetone latency stays low. (With one
// shared gate, each breath or tap switched the hiss on in the earpiece for about 0.5 s.)
// Levels are in 16-bit sample units. The hiss stays below about 60, and speech goes far above it.
#define GATE_OPEN          100                                      // about -50 dBFS
#define GATE_HOLD          (CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE / 5)   // 200 ms
#define GATE_LOOKAHEAD     (CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE / 200) // 5 ms
#define SIDETONE_GATE_OPEN 300                                      // about -41 dBFS
#define SIDETONE_GATE_HOLD (CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE / 10)  // 100 ms

typedef struct {
    int32_t env;   // Level, 8 fractional bits.
    uint32_t hold; // Samples left before the gate starts to close.
    int32_t gain;  // 0 to 32768 (unity).
} gate_t;

static gate_t usb_gate;
static gate_t sidetone_gate;
static int16_t gate_delay[GATE_LOOKAHEAD];
static uint32_t gate_idx;

// Update a gate with one sample, and return its gain.
static int32_t gate_update(gate_t *g, int32_t s, int32_t open_level, uint32_t hold) {
    // Level detector: it rises in about 1 ms and falls in about 32 ms.
    int32_t a = (s < 0 ? -s : s) << 8;
    if (a > g->env) g->env += (a - g->env) >> 4;
    else g->env -= (g->env - a) >> 9;

    if (g->env > (open_level << 8)) g->hold = hold;
    else if (g->hold) g->hold--;

    // The gain fades in over about 1 ms and out over about 64 ms, so there are no clicks.
    int32_t target = g->hold ? 32768 : 0;
    if (target > g->gain) g->gain += (target - g->gain + 15) >> 4;
    else g->gain -= (g->gain - target + 1023) >> 10;
    return g->gain;
}

static int32_t usb_gate_apply(int32_t s) {
    int32_t gain = gate_update(&usb_gate, s, GATE_OPEN, GATE_HOLD);
    int32_t delayed = gate_delay[gate_idx];
    gate_delay[gate_idx] = (int16_t)s;
    gate_idx = (gate_idx + 1) % GATE_LOOKAHEAD;
    return (delayed * gain) >> 15;
}

// Filter one sample (the average of OVERSAMPLE readings) and send it to the sidetone and USB.
static void mic_process(void) {
    // The average of the readings, in ADC counts with 8 fractional bits.
    int32_t x = (int32_t)((os_sum << 8) / OVERSAMPLE);
    os_sum = 0;
    os_count = 0;

    dc += (x - dc) >> 10;
    int32_t y = x - dc - pattern[pattern_idx];
    if (usb_gate.env < (BUZZ_LEARN_LEVEL << 8)) pattern[pattern_idx] += y >> 7;
    pattern_idx = (pattern_idx + 1) % PATTERN_LEN;
    // 12-bit signal to 16-bit sample.
    int32_t s = y >> 4;
    if (s > INT16_MAX) s = INT16_MAX;
    if (s < INT16_MIN) s = INT16_MIN;
    int32_t gated = usb_gate_apply(s);
    int32_t st_gain = gate_update(&sidetone_gate, s, SIDETONE_GATE_OPEN, SIDETONE_GATE_HOLD);
    sidetone_write((s * st_gain) >> 15, is_live);

    s = is_live ? (MIC_GATE_OFF ? s : gated) : 0;

    // If the buffer is full, drop the sample.
    if (head - tail < RING_SIZE) {
        ring[head & (RING_SIZE - 1)] = (int16_t)s;
        head++;
    }
}

static void adc_irq_handler(void) {
    while (!adc_fifo_is_empty()) {
        os_sum += adc_fifo_get();
        if (++os_count < OVERSAMPLE) continue;
        mic_process();
    }
}

void mic_init(void) {
    adc_init();
    adc_gpio_init(PIN_MIC);
    adc_select_input(PIN_MIC - 26);

    adc_fifo_setup(true, false, 1, false, false);
    adc_set_clkdiv(48000000 / PWM_HZ - 1); // One conversion takes (1 + clkdiv) cycles at 48 MHz.
    irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_irq_handler);
    adc_irq_set_enabled(true);
    irq_set_enabled(ADC_IRQ_FIFO, true);
}

void __not_in_flash_func(mic_start)(void) {
    // This runs from RAM with interrupts off, so nothing can delay the start.
    uint slice = pwm_gpio_to_slice_num(PIN_SPEAKER);
    uint32_t saved = save_and_disable_interrupts();
    pwm_hw->intr = 1u << slice;
    while (!(pwm_hw->intr & (1u << slice))) {}
    busy_wait_at_least_cycles(ADC_PHASE_CYCLES);
    adc_run(true);
    restore_interrupts(saved);
}

void mic_set_live(bool live) {
    is_live = live;
}

uint32_t mic_read(int16_t *buf, uint32_t max) {
    uint32_t n = 0;
    while (n < max && tail != head) {
        buf[n++] = ring[tail & (RING_SIZE - 1)];
        tail++;
    }
    return n;
}
