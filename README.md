<p align="center"><img src="assets/hotline-logo-1200.png" alt="Hotline logo: HOTLINE embossed on a strip of worn black label tape" width="480"></p>

# Hotline

Hotline turns an old rotary-dial telephone (here, an original red GPO 746) into a USB-C dictation phone for a Mac. Lift the handset and macOS Dictation starts, with the handset as the microphone. Put it down and Dictation stops. In one mode, the phone also presses Enter for you, so you can dictate a message and send it.

The Mac needs no software. The phone appears as a standard USB microphone and a standard USB keyboard, both called "Hotline".

<p align="center"><img src="photos/hotline.png" alt="The finished Hotline: a red rotary-dial telephone with a mode toggle switch behind the dial and a Hotline mode card in the centre of the dial" width="600"></p>

## Features

- **Handset microphone:** a new electret mic in the original handset, as a 16 kHz USB microphone.
- **Dictation by handset:** lifting and replacing the handset starts and stops macOS Dictation.
- **Three modes**, set by a toggle switch behind the dial.
- **Auto-submit:** in one mode, the phone presses Enter after Dictation stops.
- **Silent when hung up:** while the handset is down, the microphone sends only silence.
- **Sidetone:** you hear your own voice in the earpiece, like a real phone.
- **Noise reduction:** the firmware removes the buzz from USB activity, and mutes the hiss between words.
- **Status lights:** green for the connection and mode changes, red while the mic is live.

## Use

### Set up the Mac

Do this once:

1. Connect the phone to the Mac with a USB-C data cable.
2. Open System Settings → Keyboard → Dictation, and turn Dictation on.
3. Set **Shortcut** to "Press Control Key Twice".
4. Set **Microphone source** to "Hotline".
5. Open System Settings → Sound → Input, and keep your normal microphone as the default. Then other apps do not use the phone by mistake.

When you first connect the phone, macOS can show "Keyboard Setup Assistant". Close it: Hotline only sends Control and Enter, which are the same on every keyboard layout.

### Dictate

1. Click where you want the text to go.
2. Set the toggle to the mode you want:

   | Toggle | Mode | Lift the handset | Replace the handset |
   | --- | --- | --- | --- |
   | Left | Mic only | Mic on | Mic off |
   | Middle | Dictation | Mic on, Dictation starts | Mic off, Dictation stops |
   | Right | Dictation and auto-submit | Mic on, Dictation starts | Mic off, Dictation stops, Enter after 1 s |

3. Lift the handset, wait for Dictation to start, and speak.
4. Replace the handset to stop.

The phone reads the toggle when you lift or replace the handset. If Dictation was started, replacing the handset always stops it, even if you moved the toggle to "Mic only" during the call.

"Mic only" makes the phone a plain microphone, for example for calls or recordings. Select "Hotline" as the microphone in the app.

### Status lights

| Light | Meaning |
| --- | --- |
| Green, 20 s | The Mac has recognised the phone |
| Green, 2 s | You moved the toggle |
| Red | The handset is lifted and the mic is live |

### Dial card

A printable label for the centre of the dial shows the three modes.

1. Open [`assets/print-dial-card.html`](assets/print-dial-card.html) in Chrome.
2. Set the diameter of your dial centre, and press Print.
3. In the Chrome preview, click "Print using system dialog" (⌥⌘P).
4. Set the scale to 100 % and the quality to Best, and print.

The page gives the full steps.

## Make your own

You need a GPO 746 telephone (the printed parts are made for it), a few small electronic parts (from The Pi Hut and Amazon), a 3D printer, and a soldering iron. The build needs no new holes in the case, and nothing in the original phone is cut, so you can restore it later.

The [build guide](docs/build-guide.md) takes you through it step by step: the parts, the printed parts, the handset, the board, the wiring, the firmware and the test.

| Document | Contents |
| --- | --- |
| [Build guide](docs/build-guide.md) | How to make a Hotline, step by step |
| [Wiring](docs/wiring.md) | The wiring diagram and every connection |
| [Firmware](docs/firmware.md) | Build, flash, settings, how it works, troubleshooting |
| [3D-printed parts](models/README.md) | The printed parts and their print settings |

## Known issues

- **A slight crackle in the earpiece** while you talk. It is not in the recordings on the Mac.
- **A little hiss** behind your voice, about 40 dB below normal speech. It comes from the mic and its amplifier.

See [Firmware: Known issues](docs/firmware.md#known-issues) for the details.

## Repository

| Folder | Contents |
| --- | --- |
| [`firmware/`](firmware) | The firmware for the QT Py RP2040 (C, Pico SDK, TinyUSB) |
| [`docs/`](docs) | Build guide, wiring and firmware documentation |
| [`models/`](models) | STL files and print settings for the 3D-printed parts |
| [`assets/`](assets) | The logo, and the dial card with its print page |
| [`photos/`](photos) | Photos of the finished phone and its wiring |

Warning: never connect a Hotline to a telephone line. The line voltage can injure you and damage the phone and the Mac.
