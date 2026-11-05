/*
 * 固定小数点コア: セグメント → 合成器パラメータの目標値
 * ColorfulTalk V3 と同じ考え方を整数で書き、8kHz 出力に合わせて摩擦音の帯域を下げたもの。
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "fx_internal.h"

#define FMAX 3600   /* 8kHz で使える上限（ナイキストの 0.9 倍） */

/* 成人男性基準の母音: F1 F2 F3 B1 B2 B3 */
static const int16_t VF[6][6] = {
    { 500, 1500, 2500, 80, 100, 140 },
    { 750, 1250, 2600, 90, 100, 140 },  /* a */
    { 290, 2250, 3000, 60, 100, 150 },  /* i */
    { 340, 1450, 2350, 70, 100, 140 },  /* u */
    { 480, 1950, 2600, 70, 100, 150 },  /* e */
    { 480,  850, 2550, 80,  90, 140 },  /* o */
};

/* (a * num / den) を整数で */
#define MIX(base, num, den, x) (int16_t)((base) + ((int32_t)(num) * ((x) - (base))) / (den))

static void vowel_formants(CtFxFrame* f, int16_t v, const CtFxVoice* vc)
{
    const int16_t* t = VF[v >= 0 && v <= V_O ? v : 0];
    f->f[0] = t[0]; f->f[1] = t[1]; f->f[2] = t[2]; f->f[3] = 3500;
    f->b[0] = (int16_t)(t[3] + vc->b1add); f->b[1] = t[4]; f->b[2] = t[5]; f->b[3] = 250;
}

static void locus_formants(CtFxFrame* f, int16_t place, int16_t v, const CtFxVoice* vc)
{
    int16_t F2v, F3v, l;
    vowel_formants(f, v, vc);
    F2v = f->f[1];
    F3v = f->f[2];
    switch (place) {
    case PL_LAB:
        f->f[1] = MIX(800, 45, 100, F2v);
        f->f[2] = MIX(2200, 50, 100, F3v);
        break;
    case PL_ALV:
        f->f[1] = MIX(1700, 45, 100, F2v);
        f->f[2] = MIX(2650, 40, 100, F3v);
        break;
    case PL_PAL:
        f->f[1] = MIX(2150, 30, 100, F2v);
        f->f[2] = MIX(2900, 30, 100, F3v);
        break;
    case PL_VEL:
        if (v == V_I || v == V_E) { l = 2300; f->f[2] = 2700; }
        else { l = (int16_t)(F2v + 400); if (l < 1400) l = 1400; f->f[2] = (int16_t)(l + 450); }
        f->f[1] = l;
        break;
    default:
        return;
    }
    if (f->f[2] < f->f[1] + 250) f->f[2] = (int16_t)(f->f[1] + 250);
    f->f[0] = 260;
}

static void fres(CtFxFrame* f, int16_t k, int16_t hz, int16_t bw, int16_t db)
{
    f->fr_f[k] = hz;
    f->fr_b[k] = bw;
    f->fr_a[k] = fx_db(db);
}

static void fric_shape(CtFxFrame* f, int16_t cons, int16_t g)
{
    switch (cons) {
    case C_S: case C_Z: case C_TS:          /* 8kHz では帯域の上端に寄せる */
        fres(f, 0, 3500, 700, (int16_t)(54 + g));
        fres(f, 1, 2800, 500, (int16_t)(40 + g));
        f->ab = fx_db((int16_t)(42 + g));
        break;
    case C_SH: case C_J: case C_CH:
        fres(f, 0, 2700, 450, (int16_t)(53 + g));
        fres(f, 1, 3500, 800, (int16_t)(48 + g));
        break;
    case C_HY:
        fres(f, 0, 3200, 700, (int16_t)(44 + g));
        fres(f, 1, 3600, 900, (int16_t)(38 + g));
        f->ah = fx_db((int16_t)(46 + g));
        break;
    case C_F:
        fres(f, 0, 1400, 1200, (int16_t)(46 + g));
        fres(f, 1, 3200, 1500, (int16_t)(42 + g));
        f->ab = fx_db((int16_t)(38 + g));
        f->ah = fx_db((int16_t)(50 + g));
        break;
    case C_H:
        f->ah = fx_db((int16_t)(47 + g));
        f->b[0] = (int16_t)(f->b[0] * 2);
        break;
    default:
        break;
    }
}

static void burst_shape(CtFxFrame* f, int16_t cons)
{
    uint8_t ci = ct_cons_info(cons);
    if (CT_MANNER(ci) == MN_AFFR) { fric_shape(f, cons, 2); return; }
    switch (CT_PLACE(ci)) {
    case PL_LAB:
        fres(f, 0, 1000, 1500, 56);
        f->ab = fx_db(50);
        break;
    case PL_ALV:
        fres(f, 0, 3500, 1500, 58);
        fres(f, 1, 2900, 800, 50);
        break;
    case PL_VEL:
        fres(f, 0, f->f[1], 250, 63);
        fres(f, 1, f->f[2], 400, 55);
        break;
    default:
        break;
    }
}

static void nasal_shape(CtFxFrame* f, int16_t place, int16_t v, const CtFxVoice* vc)
{
    static const uint8_t LOC[4] = { PL_LAB, PL_ALV, PL_VEL, PL_VEL };
    int16_t pal = (int16_t)(place == NP_N && v == V_I);   /* に・にゃ行は硬口蓋 */
    locus_formants(f, pal ? PL_PAL : LOC[place & 3], v, vc);
    if (place == NP_M)           f->f[2] = 2130;
    else if (place == NP_N)      f->f[2] = 2600;
    else if (place == NP_UVULAR) { f->f[1] = 1250; f->f[2] = 2500; }
    f->av = fx_db(60);
    f->f[0] = 480; f->b[0] = 40;
    f->b[1] = place == NP_M ? 200 : 150;
    f->b[2] = place == NP_M ? 200 : 250;
    f->fnp = 270; f->bnp = 100;
    f->fnz = 450; f->bnz = 100;
}

void fx_seg_target(const CtFxSeg* s, const CtFxVoice* vc, int16_t fscale, CtFxFrame* f)
{
    uint8_t ci = ct_cons_info(s->cons);
    int16_t v = s->vowel ? s->vowel : V_A, k;
    uint16_t fric_scale = fx_isqrt32((uint32_t)fscale << 12);   /* sqrt(fscale) Q12 */

    memset(f, 0, sizeof *f);
    vowel_formants(f, v, vc);
    f->fnp = 270; f->bnp = 100; f->fnz = 270; f->bnz = 100;
    f->fr_f[0] = 3000; f->fr_f[1] = 3500;
    f->fr_b[0] = 500;  f->fr_b[1] = 800;

    switch (s->kind) {
    case S_SIL:
        break;
    case S_VOWEL:
        f->av = fx_db(60);
        f->ah = fx_db(vc->breath_db);
        break;
    case S_DEVVOWEL:
        f->ah = fx_db(48);
        f->b[0] = (int16_t)(f->b[0] * 5 / 2);
        if (s->cons == C_S || s->cons == C_SH || s->cons == C_TS || s->cons == C_CH || s->cons == C_HY)
            fric_shape(f, s->cons, -10);
        break;
    case S_CLOSURE:
        locus_formants(f, CT_PLACE(ci), v, vc);
        break;
    case S_VBAR:
        locus_formants(f, CT_PLACE(ci), v, vc);
        f->av = fx_db(48);
        f->f[0] = 200;
        f->b[0] = 80;
        break;
    case S_BURST:
        locus_formants(f, CT_PLACE(ci), v, vc);
        burst_shape(f, s->cons);
        break;
    case S_ASP:
        locus_formants(f, CT_PLACE(ci), v, vc);
        f->ah = fx_db(54);
        f->b[0] = (int16_t)(f->b[0] * 2);
        if (CT_PLACE(ci) == PL_VEL) fres(f, 0, f->f[1], 300, 50);
        if (CT_PLACE(ci) == PL_ALV) fres(f, 0, 3500, 1500, 44);
        break;
    case S_FRIC:
        if (CT_PLACE(ci) != PL_GLOT) locus_formants(f, CT_PLACE(ci), v, vc);
        fric_shape(f, s->cons, s->aux ? -4 : 0);
        if (s->aux) { f->av = fx_db(50); f->f[0] = 250; }
        break;
    case S_NASAL:
        nasal_shape(f, s->aux, v, vc);
        break;
    case S_FLAP:
        locus_formants(f, PL_ALV, v, vc);
        f->av = fx_db(42);
        f->f[0] = 230;
        f->f[2] = (int16_t)((int32_t)f->f[2] * 95 / 100);
        f->b[0] = 100;
        f->b[1] = 150;
        break;
    case S_GLIDE:
        if (s->aux == G_Y) { f->f[0] = 260; f->f[1] = 2200; f->f[2] = 3000; }
        else               { f->f[0] = 320; f->f[1] = 780;  f->f[2] = 2300; }
        f->av = fx_db(56);
        break;
    default:
        break;
    }

    /* ささやき声: 声帯振動を気息音に置き換える */
    if (vc->whisper) {
        int32_t a = (int32_t)f->ah + f->av;
        f->ah = (int16_t)(a > 16000 ? 16000 : a);
        f->av = 0;
        f->b[0] = (int16_t)(f->b[0] + 60);
    }

    /* 声道長に応じて伸縮し、8kHz で出せない成分を外す */
    for (k = 0; k < 4; k++) f->f[k] = (int16_t)(((int32_t)f->f[k] * fscale) >> 12);
    f->fnp = (int16_t)(((int32_t)f->fnp * fscale) >> 12);
    f->fnz = (int16_t)(((int32_t)f->fnz * fscale) >> 12);
    for (k = 0; k < 3; k++) if (f->f[k] > FMAX) f->f[k] = FMAX;
    if (f->f[3] > FMAX) f->f[3] = 0;
    for (k = 0; k < 2; k++) {
        int32_t hz = ((int32_t)f->fr_f[k] * fric_scale) >> 12;
        if (hz > FMAX) hz = FMAX;
        f->fr_f[k] = (int16_t)hz;
    }
}
