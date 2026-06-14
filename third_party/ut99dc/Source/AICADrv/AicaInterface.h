#ifndef AICA_INTERFACE_H
#define AICA_INTERFACE_H

#include <dc/sound/sound.h>
#include <dc/g2bus.h>
#include <dc/sound/aica_comm.h>

#define SPU_RAM_UNCACHED_BASE_U8 ((uint8_t *)SPU_RAM_UNCACHED_BASE)

#define AICA_MEM_CHANNELS   0x020000    /* 64 * 16*4 = 4K */

/* Quick access to the AICA channels */
#define AICA_CHANNEL(x)     (AICA_MEM_CHANNELS + (x) * sizeof(aica_channel_t))
#define AICA_MAX_SAMPLES 65534
#define AICA_PAN_LEFT 0
#define AICA_PAN_CENTER 128
#define AICA_PAN_RIGHT 255

int aica_play_chn(int chn, int size, uint32_t aica_buffer, int fmt, int vol, int pan, int loop, int freq);
void aica_stop_chn(int chn);
void aica_volpan_chn(int chn, int vol, int pan);
void aica_snd_sfx_volume(int chn, int vol);
void aica_snd_sfx_pan(int chn, int pan);
void aica_snd_sfx_freq(int chn, int freq);
void aica_snd_sfx_freq_vol(int chn, int freq, int vol);

#endif