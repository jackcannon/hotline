#pragma once

// Move mic samples to the USB audio buffer. Call it often from the main loop.
void usb_audio_task(void);
