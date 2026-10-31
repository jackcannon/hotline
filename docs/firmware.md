# Firmware

The firmware is C on the Raspberry Pi Pico SDK, with TinyUSB. It is in [`firmware/`](../firmware). This page tells you how to build and flash it, what you can change, and how it works.

## Build

You need `cmake`, `picotool` and the Arm GNU toolchain. On macOS:

```bash
brew install cmake picotool
brew install --cask gcc-arm-embedded
```

Install `picotool` with Homebrew. If the Pico SDK builds `picotool` from source, Homebrew's `mbedtls` headers in `/usr/local/include` can break that build.

From the repository root:

```bash
cmake -S firmware -B build
cmake --build build -j
```

The first build downloads Pico SDK 2.3.1, unless `PICO_SDK_PATH` points to a copy of the SDK. The firmware file is `build/hotline.uf2`.

## Flash

1. Hold the BOOT button on the QT Py.
2. Connect the QT Py to the Mac with USB. A drive called `RPI-RP2` appears.
3. Release BOOT, and copy the firmware to the drive:

   ```bash
   cp build/hotline.uf2 /Volumes/RPI-RP2/
   ```

The QT Py restarts with the new firmware, and the drive disappears.

The BOOT button is the only way to start the update mode, so you must open the phone to update the firmware. You can also hold BOOT and press RESET on the QT Py, without unplugging it.

## Check that it works

1. Open System Settings → Sound → Input. "Hotline" must be in the list.
2. Lift the handset: the red LED comes on.
3. Move the toggle: the green LED comes on for 2 s.
4. Open QuickTime Player, select File → New Audio Recording, and select "Hotline" as the microphone. Speak into the handset: the level meter moves. Put the handset down: the meter stops.

## Settings

All settings are in [`firmware/src/config.h`](../firmware/src/config.h). Change a value, then build and flash again.

| Setting | Value | Meaning |
| --- | --- | --- |
| `HOOK_LIFTED_LEVEL` | 0 | Hook switch level when the handset is lifted (0: the switch closes to ground) |
| `PIN_TOGGLE_LEFT`, `PIN_TOGGLE_RIGHT` | A, B | Swap them if left and right are the wrong way round |
| `DEBOUNCE_MS` | 30 | Time in ms that a switch must be stable before a change counts |
| `SUBMIT_DELAY_MS` | 1000 | Auto-submit: time in ms from "stop Dictation" to Enter |
| `SIDETONE_VOLUME_PERCENT` | 50 | Earpiece volume |
| `SIDETONE_LIMIT_PERCENT` | 40 | Largest earpiece signal. Keep it at 40 or less (see [Timing](#timing)) |
| `GREEN_LED_PERCENT`, `RED_LED_PERCENT` | 8, 80 | LED brightness |
| `GREEN_ON_MOUNT_MS`, `GREEN_ON_MODE_MS` | 20000, 2000 | Green LED on-times after a connection and after a toggle change |

Test switches. Keep them at 0 in normal use:

| Setting | At 1 |
| --- | --- |
| `MIC_GATE_OFF` | The USB microphone skips its noise gate, so you can record and measure the noise in silence |
| `SIDETONE_TEST_SIGNAL` | While the handset is lifted, the earpiece plays a test signal (a stuttered tone sweep, then beeps) instead of the mic |
| `LED_BRIGHTNESS_TEST` | Both LEDs stay on, so you can compare their brightness |

Caution: do not change `SYS_CLOCK_KHZ` or `PWM_HZ` unless you read [Timing](#timing) first. The wrong values bring back a whine.

## USB device

- A composite USB device called "Hotline": a UAC2 microphone (16 kHz, mono, 16-bit) and a HID boot keyboard.
- Vendor ID 0xCafe (the TinyUSB test ID) and product ID 0x4014. That is fine for a personal device.
- The keyboard sends only two things: Control twice (the macOS Dictation shortcut), and Enter.
- TinyUSB 0.18.0 has a bug on the RP2040: when an app stops the mic and starts it again, the stream can fail, and the app can crash. `usb_device.c` has a workaround. Remove it when the Pico SDK includes the upstream TinyUSB fix.

## How it works

### Modes

| File | What it does |
| --- | --- |
| `main.c` | Reads the hook switch and the toggle (with debounce), runs the modes and the LEDs |
| `keyboard.c` | Sends timed key presses: Control twice, and Enter |
| `mic.c` | Reads the ADC and filters the mic signal |
| `sidetone.c` | Drives the earpiece |
| `usb_descriptors.c`, `usb_device.c` | The USB device and its callbacks |

The mode is read when the handset is lifted or replaced. If Dictation was started, replacing the handset always stops it, even after a change to "Mic only". The microphone sends silence while the handset is down.

### Mic signal

1. The ADC reads the mic 64,000 times a second. Each 16 kHz USB sample is the average of 4 readings. This reduces the ADC noise.
2. A DC filter removes the offset of the MAX4466 output (it sits at half the supply).
3. A buzz filter learns the pattern that repeats every 1 ms (from USB activity) while the level is low, and subtracts it. It does not learn during speech, so speech cannot leave an echo in it.
4. A noise gate mutes the hiss between words. It looks 5 ms ahead, so it opens before a word starts.

### Earpiece (sidetone)

- Each mic sample goes straight to the earpiece PWM, with no USB in the path. The delay is about 0.07 ms.
- The earpiece has its own noise gate, which opens at a higher level than the USB one, so breath and taps do not switch on the hiss.
- The earpiece has no series capacitor, so the signal sits on a steady PWM level. That level follows the peaks of your voice and is 0 in silence. A fixed level carried supply ripple into the earpiece as a constant whine.

### Timing

The earpiece PWM leaks into the mic in two ways: through the cord (the red and white wires lie side by side), and through the shared green ground wire. The firmware times the mic readings so that the leak does no harm:

- The system clock is 144 MHz. The earpiece and LED PWM run at 64 kHz, which is also the ADC rate. Both clocks come from the one crystal, so they stay locked.
- The PWM is centred (phase-correct). The earpiece pulse is in the middle of each period, so its off-time is centred on the start of the period.
- The ADC reads once per period, at the centre of the earpiece off-time. There, the earpiece coil current equals its average, and no PWM edge is near.
- The firmware starts the ADC once at power-up, at the right moment; after that, hardware keeps the timing.

With other timings, the leak made a constant whine, a ping, or clicks. The earpiece edges must stay at least 2.3 µs (15 % of the period) from the period start, so keep `SIDETONE_LIMIT_PERCENT` at 40 or less.

## Known issues

- **Slight crackle in the earpiece** while you talk. It is not in the USB recordings. Three fixes were tried; each brought back a whine in the recordings, so they were reverted.
- **Hiss:** about 40 dB below normal speech. Most of it comes from the mic and the amplifier, so the firmware cannot remove it while you speak. A hardware fix that can help: a filter on the MAX4466 supply (a 470 ohm resistor in series with the blue wire at pad 3v, and about 100 µF from the blue wire to ground).

## Troubleshooting

| Problem | Check |
| --- | --- |
| A whine in the earpiece | Does it stop when you press the hook switch down? Does it change with `SIDETONE_VOLUME_PERCENT`? If it does not change with the volume, it comes from the earpiece drive, not the mic. |
| Normal speech sounds distorted | Turn the MAX4466 gain trimmer down. On this board, clockwise lowers the gain. |
| "Keyboard Setup Assistant" opens | Close it. Hotline sends only Control and Enter, which are the same on every layout. |
| Apps use the phone as their mic | Set your normal mic as the default in System Settings → Sound → Input. |
| Left and right modes are swapped | Swap `PIN_TOGGLE_LEFT` and `PIN_TOGGLE_RIGHT`. |
| The red LED is on while the handset is down | Set `HOOK_LIFTED_LEVEL` to 1. |
