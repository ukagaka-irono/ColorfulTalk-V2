/*
 * 固定小数点コア: カスケード／パラレル型フォルマント合成器（Klatt 1980 の方式を整数で実装）
 * 共振器の係数は Q13、信号は int16 で飽和させる。係数は 1 フレーム（32 サンプル）ごとに表から作る。
 * SPDX-License-Identifier: MIT
 */
#include "fx_internal.h"

#define FMAX     3680   /* これ以上の周波数の共振器は使わない */
#define SRC_SH   17     /* 音源の大きさ（60dB の母音でピーク ±512） */
#define K_ASP    77     /* 気息音 0.30 Q8 */
#define K_FRIC   51     /* 摩擦音 0.20 Q8 */
#define OUT_MUL  936    /* 出力ゲイン（経験的に調整） */

static int16_t sat16(int32_t x)
{
    return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
}

/* e = exp(-π bw/fs), cs = cos(2π f/fs) から b, c（Q13） */
static void coef(int16_t f, int16_t bw, int16_t* b, int16_t* c, int16_t* cs_out)
{
    int32_t e = fx_exp_bw(bw), cs = fx_cos_hz(f);
    *c = (int16_t)(-((e * e) >> 15));
    *b = (int16_t)((e * cs) >> 14);
    if (cs_out) *cs_out = (int16_t)cs;
}

static void res_cascade(CtFxRes* r, int16_t f, int16_t bw)
{
    if (f <= 0 || f >= FMAX) { r->a = 8192; r->b = 0; r->c = 0; return; }
    coef(f, bw, &r->b, &r->c, 0);
    r->a = (int16_t)(8192 - r->b - r->c);
}

/* パラレル用: 共振周波数でのゲインが 1 になるよう a を決める */
static void res_parallel(CtFxRes* r, int16_t f, int16_t bw)
{
    int16_t cs;
    int32_t b14, c14, sw, c2w, s2w, re, im;
    if (f <= 0 || f >= FMAX) { r->a = 0; r->b = 0; r->c = 0; return; }
    coef(f, bw, &r->b, &r->c, &cs);
    b14 = (int32_t)r->b * 2;
    c14 = (int32_t)r->c * 2;
    sw = fx_isqrt32((uint32_t)(268435456L - (int32_t)cs * cs));          /* sin w  Q14 */
    c2w = (((int32_t)cs * cs) >> 13) - 16384;                            /* cos 2w Q14 */
    s2w = (sw * cs) >> 13;                                               /* sin 2w Q14 */
    re = (16384 - ((b14 * cs) >> 14) - ((c14 * c2w) >> 14)) >> 2;       /* Q12 */
    im = (((b14 * sw) >> 14) + ((c14 * s2w) >> 14)) >> 2;
    r->a = (int16_t)(fx_isqrt32((uint32_t)(re * re + im * im)) << 1);    /* Q12 → Q13 */
}

static int16_t res_run(CtFxRes* r, int16_t x)
{
    int32_t y = ((int32_t)r->a * x + (int32_t)r->b * r->y1 + (int32_t)r->c * r->y2) >> 13;
    r->y2 = r->y1;
    r->y1 = sat16(y);
    return r->y1;
}

/* 鼻腔の零。極と同じ値のとき（鼻音でないとき）は使わない */
static void anti_set(CtFxAnti* z, int16_t f, int16_t bw, int16_t fp, int16_t bp)
{
    int16_t b, c;
    int32_t a;
    z->on = (uint8_t)(f != fp || bw != bp);
    if (!z->on || f <= 0 || f >= FMAX) { z->on = 0; return; }
    coef(f, bw, &b, &c, 0);
    a = 8192L - b - c;                             /* Q13 */
    if (a < 64) a = 64;
    z->a = (8192L * 1024L) / a;                    /* Q10。低い周波数では 16bit を超えるので 32bit で持つ */
    z->b = -((int32_t)b * 1024L) / a;
    z->c = -((int32_t)c * 1024L) / a;
}

/* s: 鼻腔の極の出力、s1/s2: その 1・2 サンプル前 */
static int16_t anti_run(const CtFxAnti* z, int16_t s, int16_t s1, int16_t s2)
{
    return sat16((z->a * s + z->b * s1 + z->c * s2) >> 10);
}

static uint16_t lfsr_step(uint16_t s)
{
    return (uint16_t)((s >> 1) ^ (uint16_t)(-(int16_t)(s & 1u) & 0xB400u));
}

/* おおよそ正規分布の雑音（一様乱数 2 つの平均） */
static int16_t noise(CtFxWork* w)
{
    int32_t a, b;
    w->lfsr = lfsr_step(w->lfsr); a = (int16_t)w->lfsr;
    w->lfsr = lfsr_step(w->lfsr); b = (int16_t)w->lfsr;
    return (int16_t)((a + b) >> 1);
}

static void set_filters(CtFxWork* w, const CtFxFrame* A)
{
    uint8_t k;
    for (k = 0; k < 4; k++) res_cascade(&w->casc[k], A->f[k], A->b[k]);
    res_cascade(&w->np, A->fnp, A->bnp);
    anti_set(&w->nz, A->fnz, A->bnz, A->fnp, A->bnp);
    for (k = 0; k < 2; k++) res_parallel(&w->par[k], A->fr_f[k], A->fr_b[k]);
}

void fx_synth_reset(CtFxWork* w)
{
    w->frame_k = -1;
    w->sub = 0;
    w->phase = 0;
    w->inc = 0;
    w->amp = 4096;
    w->lfsr = 0xACE1u;
}

static int16_t soft_limit(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a > 26214) {                                /* 0.8 FS を超えたら滑らかに抑える */
        int32_t over = a - 26214;
        a = 26214 + (over * 6553L) / (over + 6553L);
    }
    return (int16_t)(x < 0 ? -a : a);
}

int16_t fx_synth_run(CtFxWork* w, int16_t* buf, int16_t n)
{
    const CtFxVoice* v = w->v;
    uint16_t te = (uint16_t)v->te << 10;
    int16_t i;

    for (i = 0; i < n; i++) {
        int32_t av, ah, ab, fa0, fa1, x, y, p;
        int16_t g, fn, src, s = w->sub, idx, fr;
        uint16_t prev, base_inc;
        uint8_t closed, k;

        if (s == 0) {
            if (w->stage == 2 && (w->frame_k + 1) * CTFX_FRAME_MS >= w->total_ms) { w->done = 1; break; }
            w->frame_k++;
            if (w->frame_k == 0) fx_make_frame(w, 0, &w->fa, &w->inc_a);
            else { w->fa = w->fb; w->inc_a = w->inc_b; }
            fx_make_frame(w, w->frame_k + 1, &w->fb, &w->inc_b);
            set_filters(w, &w->fa);
        }

        /* フレーム内で振幅を直線補間 */
        av  = w->fa.av + ((((int32_t)w->fb.av - w->fa.av) * s) >> 5);
        ah  = w->fa.ah + ((((int32_t)w->fb.ah - w->fa.ah) * s) >> 5);
        ab  = w->fa.ab + ((((int32_t)w->fb.ab - w->fa.ab) * s) >> 5);
        fa0 = w->fa.fr_a[0] + ((((int32_t)w->fb.fr_a[0] - w->fa.fr_a[0]) * s) >> 5);
        fa1 = w->fa.fr_a[1] + ((((int32_t)w->fb.fr_a[1] - w->fa.fr_a[1]) * s) >> 5);
        base_inc = (uint16_t)(w->inc_a + ((((int32_t)w->inc_b - w->inc_a) * s) >> 5));

        /* 声帯音源（周期の頭でゆらぎを決める） */
        if (w->inc == 0) w->inc = base_inc;
        prev = w->phase;
        w->phase = (uint16_t)(w->phase + w->inc);
        if (w->phase < prev) {
            int16_t r1 = noise(w), r2 = noise(w);
            w->inc = (uint16_t)(base_inc + (((int32_t)base_inc * v->jitter * r1) >> 25));
            w->amp = (int16_t)(4096 + (((int32_t)v->jitter * 3 * r2) >> 13));
        }
        idx = (int16_t)(w->phase >> 10);
        fr = (int16_t)((w->phase >> 2) & 255);
        g = (int16_t)(v->pulse[idx] + ((((int32_t)(idx < 63 ? v->pulse[idx + 1] : 0) - v->pulse[idx]) * fr) >> 8));
        g = (int16_t)(((int32_t)g * w->amp) >> 12);
        closed = (uint8_t)(av > 0 && w->phase >= te);

        src = (int16_t)(((int32_t)g * av) >> SRC_SH);
        w->tilt_y = (int16_t)(w->tilt_y + ((((int32_t)src - w->tilt_y) * (256 - v->tilt)) >> 8));

        w->asp_lp = (int16_t)(((int32_t)w->asp_lp + noise(w)) >> 1);
        {
            int32_t asp = (((int32_t)w->asp_lp * ah) >> SRC_SH) * K_ASP >> 8;
            if (closed) asp = (asp * 102) >> 8;
            x = (int32_t)w->tilt_y + asp;
        }

        /* カスケード枝: 鼻腔の極 → 零 → F4 … F1 */
        x = sat16(x);
        {   /* 鼻腔の極は常に通し（履歴を保つ）、鼻音のときだけ零を掛ける。鼻音でなければ素通し */
            int16_t s1 = w->np.y1, s2 = w->np.y2;
            int16_t sp = res_run(&w->np, (int16_t)x);
            if (w->nz.on) x = anti_run(&w->nz, sp, s1, s2);
        }
        for (k = 4; k-- > 0;) {
            if (w->casc[k].a == 8192 && w->casc[k].b == 0) continue;   /* 使わない共振器 */
            x = res_run(&w->casc[k], (int16_t)x);
        }

        /* パラレル枝（摩擦） */
        fn = noise(w);
        if (closed) fn = (int16_t)(((int32_t)fn * 102) >> 8);
        p = ((fa0 * res_run(&w->par[0], fn)) >> SRC_SH) - ((fa1 * res_run(&w->par[1], fn)) >> SRC_SH)
          + ((ab * fn) >> SRC_SH);
        p = (p * K_FRIC) >> 8;

        y = x + p;
        {
            int32_t r = y - w->prev_y;                /* 放射特性 */
            w->prev_y = sat16(y);
            buf[i] = soft_limit((((r * (int32_t)v->gain) >> 8) * OUT_MUL) >> 4);
        }
        w->sub = (uint8_t)((s + 1) & (CTFX_FRAME - 1));
    }
    return i;
}
