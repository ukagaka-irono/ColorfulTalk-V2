/* ColorfulTalk V2: 公開 API  SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "colorful_talk_v2.h"
#include "ct2_voice.h"
#ifndef CT_NO_WAV
#  include "ct_wav.h"
#endif

/* 作業領域: 合成コアの作業領域と、写した声 */
typedef struct {
    CtFxWork  fx;
    CtFxVoice voice;
} Ct2Work;

int ct2_voice_check(const void* voice, size_t size)
{
    if (!voice) return CT_ERR_NULL;
    if (size == 0) return CT_ERR_VOICE;
    return ct2_voice_load(voice, size, 0);
}

int ct2_voice_name(const void* voice, char* name)
{
    CtFxVoice v;
    int err;
    if (!voice || !name) return CT_ERR_NULL;
    err = ct2_voice_load(voice, 0, &v);
    if (err) return err;
    memcpy(name, ((const uint8_t*)voice) + 16, 15);
    name[15] = '\0';
    return CT_OK;
}

size_t ct2_work_size(void)
{
    return sizeof(Ct2Work);
}

int ct2_start(void* work, const char* koe, const void* voice, int speed)
{
    Ct2Work* w = (Ct2Work*)work;
    CtFxVoice v;
    int err;
    if (!w || !koe || !voice) return CT_ERR_NULL;
    w->fx.magic = 0;
    w->fx.err_pos = -1;
    err = ct2_voice_load(voice, 0, &v);
    if (err) return err;
    w->voice = v;        /* 合成コアはこの写しを使う（ctfx_start が消すのは fx の部分だけ） */
    return ctfx_start(&w->fx, koe, &w->voice, (int16_t)(speed < 0 ? 0 : speed > 300 ? 301 : speed));
}

int ct2_read(void* work, int16_t* buf, int n)
{
    if (!work) return 0;
    return ctfx_read(&((Ct2Work*)work)->fx, buf, (int16_t)(n > 32767 ? 32767 : n));
}

int ct2_error_pos(const void* work)
{
    return work ? ((const Ct2Work*)work)->fx.err_pos : -1;
}

#ifndef CT_NO_WAV
unsigned char* ct2_synthe_wav(const char* koe, const void* voice, int speed, int* size, int* err)
{
    Ct2Work* w = (Ct2Work*)malloc(sizeof *w);
    CtPcm pcm = { 0 };
    unsigned char* wav = NULL;
    int e;

    if (size) *size = 0;
    if (!w) { if (err) *err = CT_ERR_MEMORY; return NULL; }
    e = !size ? CT_ERR_NULL : ct2_start(w, koe, voice, speed);
    if (!e) {
        int16_t buf[512];
        int got;
        while ((got = ct2_read(w, buf, 512)) > 0) {
            if (ct_pcm_append(&pcm, buf, got)) { e = CT_ERR_MEMORY; break; }
        }
        if (!e) {
            wav = ct_wav_build(pcm.data, pcm.n, CT2_SAMPLE_RATE, size);
            if (!wav) e = CT_ERR_MEMORY;
        }
    }
    free(pcm.data);
    free(w);
    if (err) *err = e;
    return wav;
}

void ct2_free(void* buf)
{
    free(buf);
}
#endif
