// Hotline firmware: the USB microphone, the keyboard, and the three modes.
//
// Toggle left: mic only. Middle: Dictation. Right: Dictation and auto-submit.
// The mic is live while the handset is lifted, in every mode.
// In the Dictation modes, lifting the handset starts Dictation and replacing it stops Dictation.
// In auto-submit mode, Enter follows SUBMIT_DELAY_MS after the stop.
// The mode is read when the handset is lifted or replaced. If Dictation was started,
// replacing the handset always stops it, even after a change to mic only.
//
// The earpiece plays the mic (sidetone) while the handset is lifted.
//
// Red LED: on while the handset is lifted.
// Green LED: on for GREEN_ON_MOUNT_MS after the computer recognises the device, and for
// GREEN_ON_MODE_MS after each change of the toggle position. A new change never shortens it.

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "tusb.h"
#include "config.h"
#include "keyboard.h"
#include "mic.h"
#include "sidetone.h"
#include "usb_device.h"

typedef enum { MODE_MIC_ONLY, MODE_DICTATION, MODE_AUTO_SUBMIT } phone_mode_t;

typedef struct {
    uint pin;
    bool state;        // The debounced level.
    bool raw;          // The last level read.
    uint32_t raw_since; // When raw last changed, in ms.
} debounced_t;

static void init_input(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

static debounced_t debounced_init(uint pin) {
    init_input(pin);
    bool level = gpio_get(pin);
    return (debounced_t){ .pin = pin, .state = level, .raw = level, .raw_since = 0 };
}

typedef struct {
    uint pin;
    uint16_t on_level;
    uint16_t off_level;
} led_t;

static led_t green;
static led_t red;
static absolute_time_t green_until;

// Set up PWM (PWM_HZ) on an LED pin, with the LED off. The PWM is centred (phase-correct), so
// each pulse is centred on a fixed point and its edges are at equal distances on both sides.
// At 50 % or more, the pulse is centred on the period start; below 50 %, the output is inverted,
// so the pulse is centred in the middle of the period. Either way, the edges stay at least
// 25 % of the period (3.9 us) from the ADC reading at the period start (see PWM_HZ).
// Both LED pins are on one PWM slice.
static led_t led_init(uint pin, uint32_t percent) {
    uint slice = pwm_gpio_to_slice_num(pin);
    uint16_t top = clock_get_hz(clk_sys) / PWM_HZ / 2;
    uint16_t width = top * percent / 100;
    bool invert = percent < 50;
    uint32_t inv_bits = pwm_gpio_to_channel(pin) == PWM_CHAN_A ? PWM_CH0_CSR_A_INV_BITS : PWM_CH0_CSR_B_INV_BITS;
    pwm_set_phase_correct(slice, true);
    pwm_set_wrap(slice, top - 1);
    hw_write_masked(&pwm_hw->slice[slice].csr, invert ? inv_bits : 0, inv_bits);
    led_t led = { pin, invert ? top - width : width, invert ? top : 0 };
    pwm_set_gpio_level(pin, led.off_level);
    gpio_set_function(pin, GPIO_FUNC_PWM);
    return led;
}

static void led_put(const led_t *led, bool on) {
    pwm_set_gpio_level(led->pin, on ? led->on_level : led->off_level);
}

// Keep the green LED on for at least ms from now.
static void green_on_for(uint32_t ms) {
    absolute_time_t until = make_timeout_time_ms(ms);
    if (absolute_time_diff_us(green_until, until) > 0) green_until = until;
}

void tud_mount_cb(void) {
    green_on_for(GREEN_ON_MOUNT_MS);
}

static bool debounce(debounced_t *d, uint32_t now_ms) {
    bool raw = gpio_get(d->pin);
    if (raw != d->raw) {
        d->raw = raw;
        d->raw_since = now_ms;
    } else if (raw != d->state && now_ms - d->raw_since >= DEBOUNCE_MS) {
        d->state = raw;
    }
    return d->state;
}

int main(void) {
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);

    debounced_t hook = debounced_init(PIN_HOOK);
    debounced_t toggle_left = debounced_init(PIN_TOGGLE_LEFT);
    debounced_t toggle_right = debounced_init(PIN_TOGGLE_RIGHT);
    green = led_init(PIN_LED_GREEN, GREEN_LED_PERCENT);
    red = led_init(PIN_LED_RED, RED_LED_PERCENT);

    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    tusb_init(0, &dev_init);
    sidetone_init();
    mic_init();
    // Start all PWM outputs together, so their periods line up with the ADC readings.
    pwm_set_mask_enabled((1u << pwm_gpio_to_slice_num(PIN_SPEAKER)) |
                         (1u << pwm_gpio_to_slice_num(PIN_LED_GREEN)) |
                         (1u << pwm_gpio_to_slice_num(PIN_LED_RED)));
    mic_start();

    // At power-up, the handset position does not start Dictation.
    bool was_lifted = hook.state == HOOK_LIFTED_LEVEL;
    bool dictation_on = false;
    phone_mode_t was_mode = !toggle_left.state ? MODE_MIC_ONLY
                          : !toggle_right.state ? MODE_AUTO_SUBMIT : MODE_DICTATION;

    while (true) {
        tud_task();
        usb_audio_task();
        keyboard_task();

        uint32_t now = to_ms_since_boot(get_absolute_time());
        bool lifted = debounce(&hook, now) == HOOK_LIFTED_LEVEL;
        bool left = !debounce(&toggle_left, now);
        bool right = !debounce(&toggle_right, now);
        phone_mode_t mode = left ? MODE_MIC_ONLY : right ? MODE_AUTO_SUBMIT : MODE_DICTATION;

        if (lifted && !was_lifted && mode != MODE_MIC_ONLY) {
            keyboard_dictation();
            dictation_on = true;
        } else if (!lifted && was_lifted && dictation_on) {
            keyboard_dictation();
            if (mode == MODE_AUTO_SUBMIT) {
                keyboard_wait(SUBMIT_DELAY_MS);
                keyboard_enter();
            }
            dictation_on = false;
        }
        was_lifted = lifted;

        if (mode != was_mode) green_on_for(GREEN_ON_MODE_MS);
        was_mode = mode;

        mic_set_live(lifted);
#if LED_BRIGHTNESS_TEST
        led_put(&red, true);
        led_put(&green, true);
#else
        led_put(&red, lifted);
        led_put(&green, !time_reached(green_until));
#endif
    }
}
