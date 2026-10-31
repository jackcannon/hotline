// USB class callbacks for the microphone and the keyboard.

#include "hardware/structs/usb.h"
#include "tusb.h"
#include "mic.h"
#include "usb_device.h"

// Entity IDs from TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR.
#define ENTITY_INPUT_TERMINAL 0x01
#define ENTITY_FEATURE_UNIT   0x02
#define ENTITY_CLOCK_SOURCE   0x04

// Index 0 is the master channel, index 1 is the mic channel.
static bool mute[2];
static int16_t volume[2];

static uint32_t sample_rate = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
static uint8_t clock_valid = 1;

void usb_audio_task(void) {
    int16_t buf[32];
    uint32_t n = mic_read(buf, TU_ARRAY_SIZE(buf));
    if (n == 0) return;
    if (mute[0] || mute[1]) {
        for (uint32_t i = 0; i < n; i++) buf[i] = 0;
    }
    // TinyUSB sizes each packet from how full its buffer is.
    tud_audio_write(buf, (uint16_t)(n * sizeof(buf[0])));
}

// The host stopped the mic stream.
// Workaround for TinyUSB 0.18 on RP2040: the last packet stays armed on the ISO endpoint
// (EP1 IN). When the stream starts again, TinyUSB writes over that armed buffer, and the
// stream fails. Abort the packet here. Upstream TinyUSB does this abort in
// dcd_edpt_iso_activate(). Remove this when the Pico SDK has that fix.
bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport; (void)p_request;
    uint32_t const mask = USB_EP_ABORT_EP1_IN_BITS;
    hw_set_bits(&usb_hw->abort, mask);
    while ((usb_hw->abort_done & mask) != mask) {}
    usb_dpram->ep_buf_ctrl[1].in &= ~USB_BUF_CTRL_AVAIL;
    hw_clear_bits(&usb_hw->abort_done, mask);
    hw_clear_bits(&usb_hw->abort, mask);
    return true;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    uint8_t channel = TU_U16_LOW(p_request->wValue);
    uint8_t ctrl_sel = TU_U16_HIGH(p_request->wValue);
    uint8_t entity = TU_U16_HIGH(p_request->wIndex);

    if (entity == ENTITY_INPUT_TERMINAL && ctrl_sel == AUDIO_TE_CTRL_CONNECTOR) {
        audio_desc_channel_cluster_t ret = {
            .bNrChannels = 1,
            .bmChannelConfig = (audio_channel_config_t)0,
            .iChannelNames = 0,
        };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &ret, sizeof(ret));
    }

    if (entity == ENTITY_FEATURE_UNIT && channel < TU_ARRAY_SIZE(mute)) {
        if (ctrl_sel == AUDIO_FU_CTRL_MUTE) {
            return tud_control_xfer(rhport, p_request, &mute[channel], 1);
        }
        if (ctrl_sel == AUDIO_FU_CTRL_VOLUME && p_request->bRequest == AUDIO_CS_REQ_CUR) {
            return tud_control_xfer(rhport, p_request, &volume[channel], sizeof(volume[channel]));
        }
        if (ctrl_sel == AUDIO_FU_CTRL_VOLUME && p_request->bRequest == AUDIO_CS_REQ_RANGE) {
            // Fixed at 0 dB. Units are 1/256 dB.
            audio_control_range_2_n_t(1) ret = {
                .wNumSubRanges = 1,
                .subrange[0] = { .bMin = 0, .bMax = 0, .bRes = 256 },
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &ret, sizeof(ret));
        }
    }

    if (entity == ENTITY_CLOCK_SOURCE) {
        if (ctrl_sel == AUDIO_CS_CTRL_SAM_FREQ && p_request->bRequest == AUDIO_CS_REQ_CUR) {
            return tud_control_xfer(rhport, p_request, &sample_rate, sizeof(sample_rate));
        }
        if (ctrl_sel == AUDIO_CS_CTRL_SAM_FREQ && p_request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_4_n_t(1) ret = {
                .wNumSubRanges = 1,
                .subrange[0] = { .bMin = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE,
                                 .bMax = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE, .bRes = 0 },
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &ret, sizeof(ret));
        }
        if (ctrl_sel == AUDIO_CS_CTRL_CLK_VALID) {
            return tud_control_xfer(rhport, p_request, &clock_valid, sizeof(clock_valid));
        }
    }

    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
    (void)rhport;
    uint8_t channel = TU_U16_LOW(p_request->wValue);
    uint8_t ctrl_sel = TU_U16_HIGH(p_request->wValue);
    uint8_t entity = TU_U16_HIGH(p_request->wIndex);

    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);
    TU_VERIFY(entity == ENTITY_FEATURE_UNIT && channel < TU_ARRAY_SIZE(mute));

    if (ctrl_sel == AUDIO_FU_CTRL_MUTE) {
        TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));
        mute[channel] = ((audio_control_cur_1_t *)buf)->bCur;
        return true;
    }
    if (ctrl_sel == AUDIO_FU_CTRL_VOLUME) {
        TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));
        volume[channel] = ((audio_control_cur_2_t *)buf)->bCur;
        return true;
    }
    return false;
}

// The keyboard does not answer report requests. It ignores output reports, such as the Caps Lock LED.
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}
