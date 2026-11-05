/*
 * 固定小数点コア: 4ms ごとのフレーム（境界ごとの遷移・藤崎モデルの F0）
 * ColorfulTalk V3 の ct3_frame.c を整数演算に移したもの。
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "fx_internal.h"

static uint8_t is_vowel(uint8_t k) { return k == S_VOWEL || k == S_DEVVOWEL; }
static uint8_t is_cons(uint8_t k)  { return k != S_SIL && !is_vowel(k); }

static void window(const CtFxSeg* L, const CtFxSeg* R, uint8_t g, uint8_t* pre, uint8_t* post)
{
    uint8_t l = L->kind, r = R->kind;
    *pre = 3;
    *post = 3;
    if (g == FX_G_FORM) {
        if (l == S_SIL || r == S_SIL) return;
        if (is_cons(l) && is_vowel(r)) {
            *pre = l == S_GLIDE ? 15 : 0;
            *post = l == S_FRIC ? 30 : l == S_NASAL ? 20 : l == S_FLAP ? 25 : l == S_GLIDE ? 50 : 40;
        } else if (is_vowel(l) && is_cons(r)) {
            *pre = r == S_NASAL ? 20 : r == S_FLAP ? 15 : r == S_GLIDE ? 30 : 35;
            *post = r == S_NASAL ? 5 : 0;
        } else if (is_vowel(l) && is_vowel(r)) {
            *pre = 35; *post = 35;
        } else if (l == S_NASAL || r == S_NASAL) {
            *pre = 10; *post = 10;
        }
    } else if (g == FX_G_SRC) {
        if (r == S_BURST)                                    { *pre = 1; *post = 1; }
        else if (l == S_BURST)                               { *pre = 1; *post = 2; }
        else if (is_vowel(l) && r == S_SIL)                  { *pre = 40; *post = 5; }
        else if (r == S_FRIC && (l == S_SIL || is_vowel(l))) { *pre = 8; *post = 30; }
        else if (l == S_SIL && is_vowel(r))                  { *pre = 2; *post = 10; }
        else if (is_vowel(l) && r == S_CLOSURE)              { *pre = 10; *post = 2; }
        else                                                 { *pre = 5; *post = 5; }
    }
}

#define L16(a, b, w) (int16_t)((a) + ((((int32_t)(b) - (a)) * (w)) >> 8))

static void lerp_group(CtFxFrame* o, const CtFxFrame* a, const CtFxFrame* b, int16_t w, uint8_t g)
{
    uint8_t k;
    switch (g) {
    case FX_G_FORM:
        for (k = 0; k < 4; k++) { o->f[k] = L16(a->f[k], b->f[k], w); o->b[k] = L16(a->b[k], b->b[k], w); }
        o->fnp = L16(a->fnp, b->fnp, w); o->bnp = L16(a->bnp, b->bnp, w);
        o->fnz = L16(a->fnz, b->fnz, w); o->bnz = L16(a->bnz, b->bnz, w);
        break;
    case FX_G_SRC:
        o->av = L16(a->av, b->av, w); o->ah = L16(a->ah, b->ah, w); o->ab = L16(a->ab, b->ab, w);
        for (k = 0; k < 2; k++) o->fr_a[k] = L16(a->fr_a[k], b->fr_a[k], w);
        break;
    default:
        for (k = 0; k < 2; k++) { o->fr_f[k] = L16(a->fr_f[k], b->fr_f[k], w); o->fr_b[k] = L16(a->fr_b[k], b->fr_b[k], w); }
        break;
    }
}

static int16_t amp_scale(int16_t a, int16_t lin) { return (int16_t)(((int32_t)a * lin) >> 12); }

void fx_seg_link(CtFxWork* w, int16_t ai)
{
    CtFxSeg* s = FX_SEG(w, ai);
    CtFxSeg* p = ai > w->seg_first ? FX_SEG(w, ai - 1) : 0;
    int16_t lin = fx_db((int16_t)(60 + s->gain));
    uint8_t g, k;

    fx_seg_target(s, w->v, w->fscale, &s->tg);
    s->tg.av = amp_scale(s->tg.av, lin);
    s->tg.ah = amp_scale(s->tg.ah, lin);
    s->tg.ab = amp_scale(s->tg.ab, lin);
    for (k = 0; k < 2; k++) s->tg.fr_a[k] = amp_scale(s->tg.fr_a[k], lin);

    if (s->kind == S_SIL) {
        if (p) {
            CtFxFrame keep = s->tg;
            lerp_group(&s->tg, &keep, &p->tg, 256, FX_G_FORM);
            lerp_group(&s->tg, &keep, &p->tg, 256, FX_G_FRIC);
        } else {
            s->fill_form = 1;
        }
    } else if (p && p->fill_form) {
        CtFxFrame keep = p->tg;
        lerp_group(&p->tg, &keep, &s->tg, 256, FX_G_FORM);
        lerp_group(&p->tg, &keep, &s->tg, 256, FX_G_FRIC);
        p->fill_form = 0;
    }

    for (g = 0; g < FX_G_COUNT; g++) {
        uint8_t a = 0, b = 0;
        if (p) {
            window(p, s, g, &a, &b);
            if (a > p->dur / 2) a = (uint8_t)(p->dur / 2);
            if (b > s->dur / 2) b = (uint8_t)(s->dur / 2);
        }
        s->pre[g] = a;
        s->post[g] = b;
    }
}

static uint8_t voiceless_kind(const CtFxSeg* s)
{
    return s->kind == S_ASP || s->kind == S_DEVVOWEL || s->kind == S_CLOSURE ||
           (s->kind == S_FRIC && !s->aux) || (s->kind == S_BURST && !CT_VOICED(ct_cons_info(s->cons)));
}

/* フレーム k（時刻 k × 4ms）のパラメータと、声帯音源の位相増分 */
void fx_make_frame(CtFxWork* w, int32_t k, CtFxFrame* f, uint16_t* inc)
{
    int32_t t = k * CTFX_FRAME_MS, lnf;
    const CtFxSeg *s, *p, *n;
    int16_t micro = 0, r;
    uint8_t g, i;
    uint16_t hz;

    fx_gen_fill(w, t);
    while (w->seg_cur < w->seg_end - 1 &&
           t >= FX_SEG(w, w->seg_cur)->start + FX_SEG(w, w->seg_cur)->dur)
        w->seg_cur++;
    while (w->seg_first < w->seg_cur - 1) w->seg_first++;

    s = FX_SEG(w, w->seg_cur);
    p = w->seg_cur > w->seg_first ? FX_SEG(w, w->seg_cur - 1) : 0;
    n = w->seg_cur + 1 < w->seg_end ? FX_SEG(w, w->seg_cur + 1) : 0;
    *f = s->tg;

    for (g = 0; g < FX_G_COUNT; g++) {
        if (p && t < s->start + s->post[g]) {
            int32_t st = s->start - s->pre[g];
            lerp_group(f, &p->tg, &s->tg, fx_smooth256(t - st, (int32_t)s->pre[g] + s->post[g]), g);
        } else if (n && t > n->start - n->pre[g]) {
            int32_t st = n->start - n->pre[g];
            CtFxFrame base = *f;
            lerp_group(f, &base, &n->tg, fx_smooth256(t - st, (int32_t)n->pre[g] + n->post[g]), g);
        }
    }

    /* F0（藤崎モデル, log2 Q10） */
    r = w->r > 384 ? 384 : w->r;
    lnf = -((int32_t)413 * r >> 8);
    for (i = 0; i < w->n_cmd; i++) {
        const CtFxCmd* c = &w->cmd[i];
        if (c->kind == F0_PHRASE) lnf += ((int32_t)c->amp * fx_gp(t - c->t0)) >> 12;
        else lnf += ((int32_t)c->amp * (fx_ga(t - c->t0) - fx_ga(t - c->t1))) >> 12;
    }
    /* 子音による微細な揺れ */
    if (s->kind == S_VBAR || s->kind == S_NASAL || s->kind == S_FLAP || (s->kind == S_FRIC && s->aux)) {
        micro = -52;
    } else if (s->kind == S_VOWEL && p && voiceless_kind(p)) {
        int32_t dt = t - s->start;
        micro = dt < 50 ? (int16_t)(74 * (50 - dt) / 50) : 0;
    }
    w->micro = (int16_t)(w->micro + (((int32_t)micro - w->micro) * 92 >> 8));
    lnf += w->micro;
    /* ゆっくりしたゆらぎ */
    {
        int16_t j = w->v->jitter;
        int16_t s1 = FX_SIN[(uint16_t)(t * 134 / 1000 + 13) & 63];
        int16_t s2 = FX_SIN[(uint16_t)(t * 339 / 1000 + 42) & 63];
        lnf += ((int32_t)j * (((int32_t)s1 * 18 + (int32_t)s2 * 11) >> 14)) >> 4;
    }
    hz = fx_hz_from_log2(w->v->f0, (int16_t)lnf);
    *inc = (uint16_t)(((uint32_t)hz * 8192UL) / 1000UL);

    /* フォルマントの段差を取る軽い平滑化 */
    for (i = 0; i < 4; i++) {
        if (k == 0) { w->fsm[i] = f->f[i]; w->fsm[4 + i] = f->b[i]; }
        w->fsm[i] = (int16_t)(((int32_t)w->fsm[i] * 51 + (int32_t)f->f[i] * 205) >> 8);
        w->fsm[4 + i] = (int16_t)(((int32_t)w->fsm[4 + i] * 51 + (int32_t)f->b[i] * 205) >> 8);
        f->f[i] = w->fsm[i];
        f->b[i] = w->fsm[4 + i];
    }
}
