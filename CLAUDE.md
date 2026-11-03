# Hotline

## Repository

| Path | Contents |
| --- | --- |
| `README.md` | Public overview: what Hotline is, features, Mac set-up, use, status lights, dial card, links |
| `docs/build-guide.md` | Public step-by-step build: parts and sources, printing, handset, board, base, case, wiring, flash, test |
| `docs/wiring.md` | Public wiring reference: diagram image, pin map, ProtoMate layout, ground rail, earpiece resistor, cord, hook, toggle, LEDs, USB. The source of truth for the wiring |
| `docs/firmware.md` | Public firmware guide: build, flash, settings, test switches, USB device, how it works, timing, known issues, troubleshooting |
| `docs/hotline_complete_wiring_v12.html` | The interactive wiring diagram (user's file; has two en dashes, left as they are) |
| `docs/images/wiring-diagram.png` | Light-mode render of the HTML diagram (headless Chrome, `--blink-settings=preferredColorScheme=1`, 760x1130 at 2x) |
| `firmware/` | Pico SDK project for the QT Py RP2040 (C, CMake). `src/config.h`: pins and settings; `mic.c`: ADC, filters, gates; `sidetone.c`: earpiece; `keyboard.c`: key queue; `usb_descriptors.c`, `usb_device.c`: USB |
| `models/` | STL files of the 7 printed parts, Bambu Studio projects (`PRINT SETTINGS - 0.2mm.3mf`, `- 0.4mm.3mf`, named by nozzle), and `README.md` (each part and where it goes) |
| `assets/` | Logo and dial card (design 81 from `.agent-files/logos/`): SVGs, PNGs, and `print-dial-card.html` to print the card at a set diameter |
| `photos/` | `hotline.png` (finished phone, background removed, in the README), `hotline-desk.jpg` (the same photo uncut, metadata removed), `wiring-board.jpg` and `wiring-casing.jpg` (inside views, in the build guide) |

Keep public facts (hardware, wiring, use, settings) in `README.md` and `docs/`, and point to them from here. Keep the engineering history and agent rules here, and private material in `.agent-files/`.

Build (needs `cmake`, `picotool` and the Arm GNU toolchain, `arm-none-eabi-gcc`). On macOS, run `brew install cmake picotool && brew install --cask gcc-arm-embedded`. Install `picotool` with Homebrew: if the SDK builds it from source, Homebrew's `mbedtls` headers in `/usr/local/include` break that build.

```bash
cmake -S firmware -B build && cmake --build build -j
```

CMake downloads Pico SDK 2.3.1 if `PICO_SDK_PATH` is not set. To flash, hold BOOT while you plug in the QT Py, then copy `build/hotline.uf2` to the `RPI-RP2` drive.

Firmware status (all 6 steps written; final build flashed 2026-10-29):

- The device appears on USB as "Hotline", with a UAC2 microphone (16 kHz, mono, 16-bit) and a HID boot keyboard. VID 0xCafe (TinyUSB test ID), PID 0x4014.
- TinyUSB 0.18.0 bug on RP2040: when the stream stops, the last ISO IN packet stays armed, and the next stream start writes over it; the stream then times out on the Mac (`0xE00002D6`) and apps that use the mic can crash. Workaround: `tud_audio_set_itf_close_EP_cb()` in `usb_device.c` aborts EP1 IN. Upstream TinyUSB fixes this in `dcd_edpt_iso_activate()`; remove the workaround when the Pico SDK has that fix.
- The microphone sends the handset mic audio while the handset is lifted, and silence while it is on the hook.
- The three modes work as in "Firmware behaviour" below, with debounce (30 ms) on the hook switch and the toggle. The mode is read when the handset is lifted or replaced. If Dictation was started, replacing the handset always stops it. Auto-submit presses Enter 1 s after the stop (`SUBMIT_DELAY_MS`).
- Which toggle pin is "left" is set by `PIN_TOGGLE_LEFT` in `config.h`. The direction is correct on the real board (tested 2026-10-28).
- The red LED is on while the handset is lifted.
- The green LED is on for 20 s after the computer recognises the device (USB mount), and for 2 s after each change of the toggle position. A new change never shortens a longer time that is still left. Brightness by PWM: green 8 %, red 80 % (`GREEN_LED_PERCENT`, `RED_LED_PERCENT`), chosen by the user on the real board.
- Sidetone (`sidetone.c`): each mic sample goes from the ADC interrupt to the earpiece PWM on pad 1, with no USB in the path. Latency is about 0.07 ms. It uses the same filtered signal as the USB path (see Sound quality), with its own noise gate: it opens at about -41 dBFS (`SIDETONE_GATE_OPEN`; the USB gate opens at -50 dBFS), holds for 100 ms (USB: 200 ms), and has no look-ahead. The earpiece has no series capacitor, so the signal sits on a centre (a steady PWM level). The centre follows the peaks of the signal: it jumps up at once to each new peak (plus an eighth) and falls over about 130 ms, so it is 0 in silence and while the handset is down (no PWM edges). A fixed half-on centre while the handset was lifted carried supply ripple into the earpiece as a constant whine (it stopped when the hook switch was pressed, and did not change with the sidetone volume). In simulation on recorded speech, the peak-following centre cut off no samples, and its movement above 300 Hz was 24 dB below the voice. Volume 50 %, limit 40 % (`SIDETONE_VOLUME_PERCENT`, `SIDETONE_LIMIT_PERCENT`).
- Test switches in `config.h`, all 0 in the final build: `MIC_GATE_OFF` (USB stream without the noise gate, to measure silence), `SIDETONE_TEST_SIGNAL` (earpiece plays a stuttered tone sweep and beeps in place of the mic), `LED_BRIGHTNESS_TEST` (both LEDs on).

### PWM and ADC timing

- The system clock is 144 MHz. All PWM outputs (earpiece and LEDs) run at 64 kHz, which is also the ADC rate (4 readings for each 16 kHz sample). The ADC runs by itself from its 48 MHz clock; both clocks come from the 12 MHz crystal, so they stay locked. `mic_start()` (in RAM, interrupts off) starts the ADC once, so every reading is at the same point in the period: from 0.2 us before the period start to 1.8 us after it.
- The PWM is centred (phase-correct). The earpiece pulse is centred in the middle of the period, so its off-time is centred on the period start at every pulse width; its edges stay at least 2.3 us from the period start. The red LED pulse (80 %) is centred on the period start (edges at +/-6.25 us); the green LED pulse (8 %) is centred in the middle (edges at 7.2 and 8.4 us).
- Why, in order of discovery (2026-10-28):
  1. The earpiece PWM (a 3.3 V square wave on the red cord wire) leaks into the white mic wire in the curly cord. With a free-running ADC and a 125 kHz PWM, the leak folded down to tones at 1, 2, 3 ... 7 kHz (strongest at 3 kHz = 128 - 125) at about -27 dBFS: a constant whine in the earpiece and the USB stream. Reading in step with the PWM makes the leak the same in every reading, so the DC filter removes it.
  2. Readings started from a PWM interrupt were sometimes late and landed on a settling edge: clicks, and the gate opened and closed in silence. The hardware-timed start fixed that.
  3. The earpiece current returns through the green cord wire, which is also the mic ground. A reading just after the pulse ended saw the top of the coil-current sawtooth, whose height changes with the pulse width in a curved way: an earpiece-to-mic loop of -16 dB (at 50 % volume), and a DC shift on each sound that the DC filter took about 150 ms to remove. A linear canceller (calibrated at power-up, or learning from test tones) removed only about 3 dB, so it was removed. With centred PWM and the reading in the middle of the off-time, the reading sees the average current: the loop is about -20 dB (100 Hz to 1 kHz) to -29 dB (3.7 kHz), and the DC shift is gone.
- Sidetone tests: the earpiece and its drive play a test signal cleanly (no ping). Loop gains above were measured with `SIDETONE_TEST_SIGNAL 1` and `MIC_GATE_OFF 1`, handset at the ear.

### Whine while talking (2026-10-28 to 2026-10-29)

On 2026-10-28 the user still heard a quiet whine while talking, in the earpiece and in USB recordings, with the final settings. Analysis of a recording found no steady tone during speech (narrow peaks at most about 6 dB above their neighbours), and about 4 to 6 dB more energy at 5.5 to 7.5 kHz (6 and 7 kHz bins) than speech recorded before the sidetone existed. On 2026-10-29 the user found that the whine was there in silence at the same level for sidetone volumes of 10 % and 50 %, and that it stopped when the hook switch was pressed. So it came from the fixed half-on earpiece centre, not from the mic. Fix: the peak-following centre (see Sidetone above). If a whine returns, first check whether it changes with the hook switch and with the sidetone volume.

### Other sidetone attempts that failed (2026-10-28)

- An expander in the earpiece path (gain falls with the cube of the level below about -44 dBFS), in place of a gate: the user heard a constant quiet whine (it was the fixed half-on centre, found later).
- A later ADC reading (3.5 us) with the earpiece centre at 40 %: no improvement.
- A linear leak canceller (calibrated at power-up, or learning from test tones): removed only about 3 dB, because part of the leak was curved (fixed by the centred PWM).

### Notes and tools (private, not in git)

- `.agent-files/notes/test-log.md`: every build, test, measurement and failure in order, with recordings.
- `.agent-files/tools/`: Python scripts to analyse recordings (loop gain per tone, noise, clicks, spectra), with a README.
- `.agent-files/recordings/`: the recordings from those tests.
- `.agent-files/checkpoints/`: saved states. "Go back to our checkpoint" means `2026-10-29-peak-centre`.

### Sidetone crackle (open)

The checkpoint (`.agent-files/checkpoints/2026-10-29-peak-centre/`, restored 2026-10-29) has a slight crackle in the earpiece while talking, and no whine in USB recordings. The user prefers that to a whine. Three fixes were tried and reverted, because each brought the whine back into the USB recordings: a 0.5 ms delay with a smooth centre rise, a 4-sample centre rise, and noise shaping of the earpiece PWM. Details and the noise-shaping patch: `.agent-files/notes/sidetone-crackle-experiments.md`.

## Project

Retrofit of an old landline telephone into a USB-C device for a Mac. Lifting the handset starts macOS Dictation and the handset mic becomes the input; replacing the handset stops Dictation and (in one mode) presses Enter to submit.

Project name: **Hotline** (confirmed by the user). Use "Hotline" as the USB device name so it appears as "Hotline" in macOS microphone and keyboard menus.

Hard requirement: **no software installed on the Mac.** The device must enumerate as standard class-compliant USB devices only.

## Architecture

A single Adafruit QT Py RP2040 acts as a **composite USB device**:

- **USB Audio Class (UAC) microphone**: samples the handset mic via its ADC and streams it to the Mac.
- **USB HID keyboard**: sends the Dictation shortcut, Enter, etc.

One USB-C cable to the Mac carries power and both data functions. CircuitPython cannot act as a USB mic, so firmware will be **C/C++ on the Pico SDK with TinyUSB** (composite UAC2 + HID). The RP2040 has two cores: use one for USB, one for audio/sidetone/IO if helpful.

The QT Py also drives the handset **speaker** (sidetone, and future tones) via PWM.

## Hardware and wiring

The parts list with sources is in `docs/build-guide.md`, and the full wiring is in `docs/wiring.md`. Rules:

- The pin map is final, chosen by the user. Do not change it without asking.
- Pad 8 (SCK, GPIO6) is the user's; leave it alone. Pads 2, 9 and TX are spare.
- The ProtoMate silkscreen uses XIAO pin names. GPIO numbers for RX, TX, SDA, SCL, SCK, MI and MO match the Pico SDK board file `adafruit_qtpy_rp2040.h`; A0 to A3 follow Adafruit's pinout.
- The green cord wire is both the mic ground and the earpiece return (the cord has only 4 wires). This shared ground is the root of the sidetone leak problems below.

Build facts from the user (2026-10-31): the phone is an original red GPO 746. The electret capsule stays on the MAX4466 board. The QT Py plugs into the ProtoMate on headers. No new holes in the case: the toggle and both LEDs are in the Top Panel, in the existing square hole in the top of the case. All fixings are M2 x 8 mm self-tapping hex socket head screws (the handover's M2.5 nylon screws were not used). Nothing in the old circuit was cut: the hook switch wires use crimped fork lugs under screw terminals 19 and 19A, like the original wiring, and the original circuit stays connected (harmless with no telephone line; the handover's "isolate the hook switch" advice was not followed, and the hook switch works).

Bought but no longer planned: a 16 mm red momentary pushbutton (cancel button dropped), a 300-piece 3 mm LED kit, spare 470 ohm resistors.

## Firmware behaviour (specification from the handover)

### Modes (3-position toggle)

| Position | Mode | Handset lifted | Handset replaced |
| --- | --- | --- | --- |
| Left | Mic only | Mic live; no keys sent | Mic silent; no keys sent |
| Middle | Dictation | Mic live; send Dictation shortcut | Mic silent; send Dictation shortcut (stop) |
| Right | Dictation + auto-submit | As Dictation | Mic silent; stop Dictation, short delay, press **Enter** |

In every mode, the mic is silent while the handset is on the hook: no audio reaches the computer (stream silence, see Audio).

The middle position reads both toggle pins high. Which outer pin (pad 10 or RX) corresponds to "left" is unknown until tested (toggle levers usually connect the opposite-side pin); make the mapping a single easy-to-flip constant.

Dictation shortcut: **Control pressed twice** (Fn/Globe cannot be sent by a standard USB HID keyboard).

### Audio

- Sample A0 via ADC at a voice-suitable rate (e.g. 16 kHz mono, 16-bit), remove DC offset (MAX4466 output sits at VCC/2), send over UAC.
- Stream **silence** whenever the handset is down, so apps that pick this mic by mistake hear nothing.
- **Sidetone:** while the handset is up and the mic is live, pass each mic sample directly to the speaker PWM on pad 1, with latency well under 1 ms (do not route through USB buffers). PWM carrier well above hearing. Software volume control and a cap to prevent feedback howl. Silent when the handset is down.
- Future: dial tone on pickup, beeps on mode change, etc., all via the same PWM.

### LEDs

| LED | Pad | Behaviour |
| --- | --- | --- |
| Green | SDA | On for 20 s once USB is mounted/recognised by the host; also on for 2 s each time the mode toggle changes position (restart the timer if already on) |
| Red | SCL | Listening: on while the handset is up and the mic is live |

### General

- Hook-switch polarity (open vs closed when lifted) must be configurable.
- Debounce all switch inputs (old hook contacts can be bouncy).
- USB product name: **"Hotline"** (shown in macOS menus).
- Firmware updates: use the BOOT button on the QT Py only (decided by the user, 2026-10-28). Do not add a bootloader shortcut with the handset or the toggle.

## Mac set-up and LEDs

Mac set-up (Dictation shortcut "Press Control Key Twice", microphone source "Hotline", keep the normal mic as the default input) is in `README.md`. LED facts (12 V prewired with a built-in resistor of about 500 ohm, coloured lead +) are in `docs/wiring.md`.

## Sound quality

Measurements from 2026-10-28, with the handset lifted. Levels are in dBFS (decibels relative to full scale).

### Done

| Measure | Where | Effect |
| --- | --- | --- |
| MAX4466 gain at minimum (25x) | Gain trimmer, fully clockwise (clockwise lowers the gain) | At maximum gain, speech clipped at both rails. At 25x, speech peaks at about -4 to -8 dBFS, with no clipping. |
| DC filter, then buzz filter (learns the 1 ms pattern only while the level is below about -41 dBFS) | `mic.c` | The DC filter (cutoff about 2.5 Hz) removes the MAX4466 offset. The buzz filter subtracts the USB buzz at every 1 kHz; while it learns, it equals a comb notch filter (notches 2.5 Hz wide at 0 to 8 kHz). The tones went from about 20 dB to 1 to 6 dB above the hiss. Total noise went from -55 to -61 dBFS. History: (1) a comb notch filter that learned all the time left a fading echo at 1, 2, 3 ... 8 kHz for about 0.13 s after each sudden end of sound (about 17 dB above the hiss in simulation): the user heard a metallic ping in the earpiece. (2) A simple DC filter in the earpiece path removed the ping but let the buzz through as a whine. (3) Learning only while the noise gate was closed, with no separate DC filter, locked up: the DC offset kept the gate open, so it never learned (constant whine). The current design simulates with no ping after speech and converges within 1 s of power-up. |
| 4x oversampling (ADC at 64 kHz, average of 4) | `mic.c` | It reduces the ADC part of the hiss. Total noise went from -61 to -64 dBFS. The tones are now lost in the hiss. |
| Noise gate (opens at about -50 dBFS, 5 ms look-ahead, 200 ms hold) | `mic.c` | It mutes the hiss between words. It does not change the hiss during speech. |

Result: speech (loudest parts, about -24 dBFS) is about 40 dB above the hiss (-64 dBFS). A click check found no lost or repeated samples in the USB stream.

### Remaining hiss

About two thirds of the remaining hiss is analogue (mic and MAX4466), and about one third is the ADC. More averaging gives less than 1 dB. The likely analogue source is the QT Py 3.3 V supply: the MAX4466 board feeds the mic capsule from it, so supply noise is amplified 25x.

### Options for the future (not done; the user chose to stop here)

1. RC filter on the MAX4466 supply: a 470 ohm resistor in series with the blue wire at the 3v pad, and about 100 uF from the blue wire to ground. This helps only if the supply is the source. To test first: power the MAX4466 from 2 AA batteries (3 V) instead of the 3v pad, and measure the hiss in silence.
2. Low-pass filter at about 4 kHz in firmware. It removes about a third of the hiss, but the voice sounds like a telephone and Dictation can be less accurate.
3. Spectral noise reduction in firmware. It reduces hiss during speech, but it adds 16 to 32 ms of latency and can add artefacts that hurt Dictation.
4. PDM MEMS mic (see Fallback plan). It removes the ADC and supply hiss, but it needs a cord wiring change.

## Fallback plan

Analog mic (MAX4466 → ADC) first. If dictation accuracy is poor after tuning the MAX4466 gain trimmer, switch to an Adafruit **PDM MEMS mic** in the handset. Note this would need all 4 cord wires (3V, GND, CLK, DATA), which conflicts with the speaker now using red; revisit if needed.

## Safety notes

The warnings (never connect to a telephone line; screws and bare wire away from the metal base) are in `docs/build-guide.md` and `docs/wiring.md`.
