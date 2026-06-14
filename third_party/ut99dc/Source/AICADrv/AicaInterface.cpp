#include <kos.h>

#include "AicaInterface.h"

int aica_play_chn(int chn, int size, uint32_t aica_buffer, int fmt, int vol, int pan, int loop, int freq) {
	// assert(size <= 65534);
	// We gotta fix this at some point
	if (size >= 65535) {
		printf("aica_play_chn: size too large for %p, %d, truncating to 65534\n", (void*)aica_buffer, size);
		size = 65534;
	}

    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_START;
    chan->base = aica_buffer;
    chan->type = fmt;
    chan->length = size;
    chan->loop = loop;
    chan->loopstart = 0;
    chan->loopend = size;
    chan->freq = freq;
    chan->vol = vol;
    chan->pan = pan;
	snd_sh4_to_aica(tmp, cmd->size);
    return chn;
}

void aica_stop_chn(int chn) {
	AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_STOP;
    snd_sh4_to_aica(tmp, cmd->size);
}

void aica_volpan_chn(int chn, int vol, int pan) {
	AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_PAN | AICA_CH_UPDATE_SET_VOL;
    chan->vol = vol;
	chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}


void aica_snd_sfx_volume(int chn, int vol) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_VOL;
    chan->vol = vol;
    snd_sh4_to_aica(tmp, cmd->size);
}

void aica_snd_sfx_pan(int chn, int pan) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_PAN;
    chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}

void aica_snd_sfx_freq(int chn, int freq) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ;
    chan->freq = freq;
    snd_sh4_to_aica(tmp, cmd->size);
}


void aica_snd_sfx_freq_vol(int chn, int freq, int vol) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ | AICA_CH_UPDATE_SET_VOL;
    chan->freq = freq;
	chan->vol = vol;
    snd_sh4_to_aica(tmp, cmd->size);
}