// Sidetone: the mic signal goes to the earpiece through PWM, with no USB in the path.

#include <math.h>
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "config.h"
#include "sidetone.h"
#include "tusb_config.h"

#if SIDETONE_TEST_SIGNAL
// Test signal in place of the mic, at half of full scale. It repeats every 12 s:
// 8 s: a sweep from 100 Hz to 4 kHz, which stutters: 100 ms on and 100 ms off, with sharp ends.
// 4 s: 1 kHz beeps, 50 ms on and 450 ms off.
#define FS          CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE
#define SWEEP_TIME  (8 * FS)
#define CYCLE_TIME  (12 * FS)
static int16_t sine_table[256];
static uint32_t phase;      // 32-bit phase: 2^32 is one cycle.
static uint32_t t;          // Samples since the start of the cycle.
static float sweep_hz;

static int32_t test_sample(void) {
    float hz;
    if (t < SWEEP_TIME) {
        // Every 1 ms, raise the frequency by the step that gives 100 Hz to 4 kHz in 8 s.
        if (t == 0) sweep_hz = 100.0f;
        else if (t % 16 == 0) sweep_hz *= powf(40.0f, 16.0f / SWEEP_TIME);
        hz = sweep_hz;
    } else {
        hz = 1000.0f;
    }
    phase += (uint32_t)(hz * (4294967296.0f / FS));
    int32_t s = sine_table[phase >> 24] / 2;
    if (t < SWEEP_TIME && t % (FS / 5) >= FS / 10) s = 0;
    if (t >= SWEEP_TIME && (t - SWEEP_TIME) % (FS / 2) >= FS / 20) s = 0;
    t = (t + 1) % CYCLE_TIME;
    return s;
}
#endif

static int32_t top;       // PWM level for a pulse that fills the whole period.
static int32_t rest;      // The highest output centre: half of top.
static int32_t gain;      // PWM counts for a full-scale sample, 15 fractional bits.
static int32_t limit;     // Largest change from the centre, in PWM counts.
static int32_t env;       // Peak size of the earpiece signal, PWM counts with 8 fractional bits.

void sidetone_init(void) {
    // main.c starts the PWM, together with the LEDs. The PWM is centred (phase-correct: the
    // counter counts up to top and back down), and the output is inverted, so each pulse is
    // centred in the middle of the period and the off-time is centred on the period start (see PWM_HZ).
    uint slice = pwm_gpio_to_slice_num(PIN_SPEAKER);
    top = clock_get_hz(clk_sys) / PWM_HZ / 2;
    pwm_set_phase_correct(slice, true);
    pwm_set_wrap(slice, top - 1);
    pwm_set_output_polarity(slice, true, true);
    pwm_set_gpio_level(PIN_SPEAKER, top);
    gpio_set_function(PIN_SPEAKER, GPIO_FUNC_PWM);

    // Volume and limit are shares of half the period.
    gain = top / 2 * SIDETONE_VOLUME_PERCENT / 100;
    limit = top / 2 * SIDETONE_LIMIT_PERCENT / 100;
    // The earpiece has no series capacitor, so the signal must sit on a centre (a steady PWM
    // level). The centre follows the size of the signal, up to rest (see sidetone_write).
    // At most the output is high for 30 % to 70 % of the period, so the edges stay at least
    // 15 % of the period (2.3 us) from the period start (see PWM_HZ).
    rest = top / 2;

#if SIDETONE_TEST_SIGNAL
    for (int i = 0; i < 256; i++) sine_table[i] = (int16_t)(32767.0f * sinf(i * 6.2831853f / 256));
#endif
}

void sidetone_write(int32_t sample, bool live) {
#if SIDETONE_TEST_SIGNAL
    if (live) sample = test_sample();
#endif
    int32_t v = (sample * gain) >> 15;
    if (v > limit) v = limit;
    if (v < -limit) v = -limit;
    if (!live) v = 0;

    // The centre follows the peaks of the signal: it jumps up at once to each new peak and falls
    // over about 130 ms, and it is an eighth above the peak so the signal always fits. In silence
    // it is 0, so the output does not switch: a steady half-on level carried supply ripple into
    // the earpiece as a constant whine. In simulation on recorded speech, no samples were cut
    // off, and the part of the centre's movement above 300 Hz was 24 dB below the voice.
    // It also fades the earpiece out when the handset is replaced.
    int32_t a = (v < 0 ? -v : v) << 8;
    if (a > env) env = a;
    else env -= (env + 2047) >> 11;
    int32_t centre = env + (env >> 3);
    if (centre > (rest << 8)) centre = rest << 8;

    // The share of the period that the output is high. The output is inverted, so the PWM
    // level is the time in the period when the pulse starts.
    int32_t high = (centre >> 8) + v;
    if (high < 0) high = 0;
    if (high > top) high = top;
    pwm_set_gpio_level(PIN_SPEAKER, (uint16_t)(top - high));
}
