/* 固定小数点コア: 開始と読み出し  SPDX-License-Identifier: MIT */
#include <string.h>
#include "fx_internal.h"

int ctfx_start(CtFxWork* w, const char* koe, const CtFxVoice* v, int16_t speed)
{
    int pos = -1, err;
    if (!w || !koe || !v) return CT_ERR_NULL;
    memset(w, 0, sizeof *w);
    w->err_pos = -1;
    if (speed < 50 || speed > 300) return CT_ERR_PARAM;

    w->koe.mora = w->mora;
    w->koe.max_mora = CTFX_MAX_MORA;
    w->koe.phrase = w->phrase;
    w->koe.max_phrase = CTFX_MAX_PHRASE;
    err = ct_koe_parse(koe, &w->koe, &pos);
    if (err) { w->err_pos = (int16_t)pos; return err; }

    w->v = v;
    w->fscale = (int16_t)v->fscale;
    w->csc = fx_speed_scale(FX_CSC, speed);
    w->vsc = fx_speed_scale(FX_VSC, speed);
    w->psc = (uint16_t)(25600 / speed);
    w->r = (int16_t)(v->f0range * 4);          /* ×1/64 → Q8 */
    w->open_acc = -1;
    w->prev_vowel = V_A;
    w->ds = 256;
    fx_synth_reset(w);
    fx_gen_fill(w, 0);
    w->magic = FX_MAGIC;
    return CT_OK;
}

int ctfx_read(CtFxWork* w, int16_t* buf, int16_t n)
{
    if (!w || !buf || n <= 0 || w->magic != FX_MAGIC || w->done) return 0;
    return fx_synth_run(w, buf, n);
}
