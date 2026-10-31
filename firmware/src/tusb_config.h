#pragma once

// TinyUSB configuration: one UAC2 microphone and one HID keyboard.
// The Pico SDK defines CFG_TUSB_MCU.

#define CFG_TUSB_OS            OPT_OS_PICO
#define CFG_TUD_ENABLED        1
#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_AUDIO 1
#define CFG_TUD_HID   1
#define CFG_TUD_CDC    0
#define CFG_TUD_MSC    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0

// Microphone: 16 kHz, mono, 16-bit.
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE           16000
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN              TUD_AUDIO_MIC_ONE_CH_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT              1
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ           64
#define CFG_TUD_AUDIO_ENABLE_EP_IN                 1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX 2
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX         1
#define CFG_TUD_AUDIO_EP_SZ_IN                     TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX          CFG_TUD_AUDIO_EP_SZ_IN
// TinyUSB keeps this buffer about half full, so it adds about 2 ms of latency.
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ       (4 * CFG_TUD_AUDIO_EP_SZ_IN)

// Keyboard: one 8-byte boot keyboard report.
#define CFG_TUD_HID_EP_BUFSIZE 8
