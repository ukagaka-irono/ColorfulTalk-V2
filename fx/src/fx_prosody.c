/*
 * 固定小数点コア: 韻律（モーラ → セグメント列と藤崎モデルの指令）
 * ColorfulTalk V3 の ct3_prosody.c を整数演算に移したもの。時間は ms の整数、
 * 声の高さの指令は log2 の Q10（1024 = 1 オクターブ）。
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "fx_internal.h"

#define MORA_MS  125
#define LEAD_MS   60
#define TRAIL_MS 150
#define T_INF    0x3FFFFFFFL

/* ln → log2 Q10 にした指令の大きさ（V3 の値 × 1477） */
#define A_ACC     620   /* 0.42 */
#define A_FLAT    443   /* 0.30 */
#define A_PHRASE  443   /* 0.30 */
#define A_COMMA   266   /* 0.18 */
#define A_FINAL  (-177) /* -0.12 */
#define A_QUEST  1108   /* 0.75 */

static int16_t scale(int16_t ms, uint16_t q8) { return (int16_t)(((int32_t)ms * q8) >> 8); }
static int16_t pct(int16_t ms, int16_t p)     { return (int16_t)(((int32_t)ms * p) / 100); }
static int16_t rmul(int16_t a, int16_t r)     { return (int16_t)(((int32_t)a * r) >> 8); }

static void add_cmd(CtFxWork* w, uint8_t kind, int32_t t0, int32_t t1, int16_t amp)
{
    CtFxCmd* c;
    if (amp == 0) return;
    if (w->n_cmd == CTFX_CMDQ) {
        memmove(&w->cmd[0], &w->cmd[1], (CTFX_CMDQ - 1) * sizeof w->cmd[0]);
        w->n_cmd--;
        w->open_acc = (int8_t)(w->open_acc > 0 ? w->open_acc - 1 : -1);
    }
    c = &w->cmd[w->n_cmd++];
    c->kind = kind;
    c->t0 = t0;
    c->t1 = t1;
    c->amp = amp;
}

static void add_seg(CtFxWork* w, uint8_t kind, uint8_t cons, uint8_t vowel, uint8_t aux, int16_t dur, int16_t gain)
{
    CtFxSeg* s;
    int32_t decl;
    if (w->seg_end - w->seg_first >= CTFX_SEGQ) w->seg_first++;
    if (dur < CTFX_FRAME_MS) dur = CTFX_FRAME_MS;
    s = FX_SEG(w, w->seg_end);
    memset(s, 0, sizeof *s);
    s->kind = kind;
    s->cons = cons;
    s->vowel = vowel;
    s->aux = aux;
    s->start = w->gen_t;
    s->dur = dur;
    decl = (w->gen_t - w->sent_t0) * 5 / 2000;      /* 2.5dB/s、最大 6dB */
    if (decl > 6) decl = 6;
    if (decl < 0) decl = 0;
    s->gain = (int8_t)(gain - decl);
    fx_seg_link(w, w->seg_end);
    w->seg_end++;
    w->gen_t += dur;
}

static uint8_t nasal_place(const CtMora* nx)
{
    uint8_t ci;
    if (!nx || nx->type != M_NORMAL || nx->cons == C_NONE) return NP_UVULAR;
    ci = ct_cons_info(nx->cons);
    if (CT_MANNER(ci) == MN_FRIC) return NP_UVULAR;
    switch (CT_PLACE(ci)) {
    case PL_LAB: return NP_M;
    case PL_ALV: case PL_PAL: return NP_N;
    case PL_VEL: return NP_NG;
    default: return NP_UVULAR;
    }
}

static int16_t fric_ms(uint8_t c)
{
    return c == C_S ? 95 : c == C_SH ? 105 : c == C_HY ? 80 : 75;
}
static int16_t vowel_len(uint8_t v)  { return v == V_A ? 105 : v == V_I ? 90 : v == V_U ? 88 : 100; }
static int16_t vowel_db(uint8_t v)   { return v == V_O || v == V_E ? -1 : v == V_I || v == V_U ? -3 : 0; }
static int16_t pause_ms(uint8_t e)   { return e == PE_COMMA ? 220 : (e == PE_PERIOD || e == PE_QUESTION) ? 420 : 0; }
static int16_t final_len(uint8_t e)  { return e == PE_NONE ? 106 : e == PE_COMMA ? 120 : e == PE_QUESTION ? 125 : 135; }

/* 子音部（＋半母音）。標準話速での長さ [ms] を返す */
static int16_t emit_cons(CtFxWork* w, const CtMora* m, uint8_t after_q, int16_t stretch, int16_t g)
{
    uint8_t ci = ct_cons_info(m->cons);
    uint8_t cv = m->glide == G_Y ? V_I : m->glide == G_W ? V_U : m->vowel;
    int16_t d0 = 0, d1 = 0, d2 = 0;
    uint16_t s = w->csc;

    switch (CT_MANNER(ci)) {
    case MN_STOP:
        if (CT_VOICED(ci)) {
            d0 = after_q ? 20 : 32; d1 = CT_PLACE(ci) == PL_VEL ? 12 : 6;
            add_seg(w, S_VBAR, m->cons, cv, 0, scale(d0, s), g);
            add_seg(w, S_BURST, m->cons, cv, 0, scale(d1, s), g);
        } else {
            d0 = after_q ? 15 : 32;
            d1 = CT_PLACE(ci) == PL_VEL ? 10 : CT_PLACE(ci) == PL_LAB ? 5 : 6;
            d2 = CT_PLACE(ci) == PL_LAB ? 25 : CT_PLACE(ci) == PL_ALV ? 20 : 32;
            add_seg(w, S_CLOSURE, m->cons, cv, 0, scale(d0, s), g);
            add_seg(w, S_BURST, m->cons, cv, 0, scale(d1, s), g);
            add_seg(w, S_ASP, m->cons, cv, 0, pct(scale(d2, s), stretch), g);
        }
        break;
    case MN_AFFR:
        if (CT_VOICED(ci)) {
            d0 = after_q ? 15 : 22; d1 = 4; d2 = 45;
            add_seg(w, S_VBAR, m->cons, cv, 0, scale(d0, s), g);
            add_seg(w, S_BURST, m->cons, cv, 0, scale(d1, s), g);
            add_seg(w, S_FRIC, m->cons, cv, 1, scale(d2, s), g);
        } else {
            d0 = after_q ? 15 : 35; d1 = 4; d2 = m->cons == C_TS ? 40 : 45;
            add_seg(w, S_CLOSURE, m->cons, cv, 0, scale(d0, s), g);
            add_seg(w, S_BURST, m->cons, cv, 0, scale(d1, s), g);
            add_seg(w, S_FRIC, m->cons, cv, 0, pct(scale(d2, s), stretch), g);
        }
        break;
    case MN_FRIC:
        d0 = fric_ms(m->cons);
        add_seg(w, S_FRIC, m->cons, cv, 0, pct(scale(d0, s), stretch), g);
        break;
    case MN_NASAL:
        d0 = 65;
        add_seg(w, S_NASAL, m->cons, cv, m->cons == C_M ? NP_M : NP_N, scale(d0, s), g);
        break;
    case MN_FLAP:
        d0 = 18;
        add_seg(w, S_FLAP, m->cons, cv, 0, scale(d0, s), g);
        break;
    default:
        break;
    }
    if (m->glide) {
        int16_t gl = m->cons ? 35 : 50;
        d2 = (int16_t)(d2 + gl);
        add_seg(w, S_GLIDE, m->cons, m->vowel, m->glide, scale(gl, s), g);
    }
    return (int16_t)(d0 + d1 + d2);
}

static void gen_mora(CtFxWork* w)
{
    const CtKoe* k = &w->koe;
    int16_t i = w->gi;
    const CtMora* m = &k->mora[i];
    const CtPhrase* ph = &k->phrase[m->phrase];
    int16_t idx = (int16_t)(i - ph->first + 1), n = (int16_t)ph->count, acc = ph->accent;
    uint8_t last_in_phrase = idx == n;
    uint8_t last_phrase = m->phrase == k->n_phrase - 1;
    const CtMora* nx = (last_in_phrase && ph->end != PE_NONE) || i + 1 >= (int16_t)k->n_mora ? 0 : &k->mora[i + 1];
    uint8_t after_q = i > 0 && k->mora[i - 1].type == M_Q;
    int16_t len = last_in_phrase ? final_len(ph->end) : 100;
    int16_t stress = idx == acc ? 108 : 100;
    int16_t fin_db = last_in_phrase && ph->end != PE_NONE ? -2 : 0;
    int32_t t0, t1;
    int16_t cms, vdur, r = w->r;

    if (idx == 1) w->aa = rmul(rmul(acc > 0 ? A_ACC : A_FLAT, r), w->ds);

    t0 = w->gen_t;
    switch (m->type) {
    case M_NORMAL: {
        int16_t g = (int16_t)(vowel_db(m->vowel) + fin_db);
        cms = emit_cons(w, m, after_q, m->devoice ? 115 : 100, g);
        t0 = w->gen_t;
        if (m->devoice) {
            add_seg(w, S_DEVVOWEL, m->cons, m->vowel, 0, scale(45, w->vsc), g);
        } else {
            vdur = (int16_t)(MORA_MS - cms / 2);
            if (vdur < 55) vdur = 55;
            vdur = pct(pct(scale(pct(vdur, vowel_len(m->vowel)), w->vsc), len), stress);
            add_seg(w, S_VOWEL, 0, m->vowel, 0, vdur, g);
        }
        w->prev_vowel = m->vowel;
        break;
    }
    case M_N:
        add_seg(w, S_NASAL, 0, w->prev_vowel, nasal_place(nx),
                pct(pct(scale(nx ? 95 : 105, w->vsc), len), stress), (int16_t)(-2 + fin_db));
        break;
    case M_LONG:
        add_seg(w, S_VOWEL, 0, m->vowel, 0, pct(pct(scale(100, w->vsc), len), stress),
                (int16_t)(vowel_db(m->vowel) - 1 + fin_db));
        break;
    case M_Q: {
        uint8_t ci = nx ? ct_cons_info(nx->cons) : 0;
        uint8_t nv = nx ? nx->vowel : w->prev_vowel;
        if (nx && nx->type == M_NORMAL && (CT_MANNER(ci) == MN_STOP || CT_MANNER(ci) == MN_AFFR))
            add_seg(w, S_CLOSURE, nx->cons, nv, 0, scale(85, w->csc), 0);
        else if (nx && nx->type == M_NORMAL && CT_MANNER(ci) == MN_FRIC)
            add_seg(w, S_FRIC, nx->cons, nv, 0, scale(70, w->csc), 0);
        else
            add_seg(w, S_SIL, 0, w->prev_vowel, 0, scale(60, w->csc), 0);
        break;
    }
    default:
        break;
    }
    t1 = w->gen_t;

    /* アクセント指令（東京方言） */
    if (acc == 1) {
        if (idx == 1) add_cmd(w, F0_ACCENT, t0 - 30, t1 - 30, w->aa);
    } else {
        if (idx == 2 || (n == 1 && idx == 1)) {
            int32_t s0 = idx == 2 ? t0 - 30 : t0 + (t1 - t0) * 3 / 10;
            add_cmd(w, F0_ACCENT, s0, T_INF, w->aa);
            w->open_acc = (int8_t)(w->aa != 0 ? w->n_cmd - 1 : -1);
        }
        if (w->open_acc >= 0 && (idx == acc || idx == n)) {
            w->cmd[w->open_acc].t1 = acc > 0 ? t1 - 30 : t1 - 10;
            w->open_acc = -1;
        }
    }

    if (last_in_phrase) {
        if (acc > 0) w->ds = (int16_t)((w->ds * 3) >> 2);
        if (ph->end == PE_QUESTION) {
            add_cmd(w, F0_ACCENT, t0 + (t1 - t0) * 15 / 100, t1 + 80, rmul(A_QUEST, r > 128 ? r : 128));
        } else if (ph->end == PE_PERIOD) {
            add_cmd(w, F0_PHRASE, t0 - 40, 0, rmul(A_FINAL, r));
        }
        if (last_phrase) {
            int16_t pm = scale(pause_ms(ph->end), w->psc);
            add_seg(w, S_SIL, 0, w->prev_vowel, 0, pm > TRAIL_MS ? pm : TRAIL_MS, 0);
        } else if (ph->end != PE_NONE) {
            add_seg(w, S_SIL, 0, k->mora[ph[1].first].vowel, 0, scale(pause_ms(ph->end), w->psc), 0);
            if (ph->end == PE_COMMA) {
                add_cmd(w, F0_PHRASE, w->gen_t - 60, 0, rmul(A_COMMA, r));
                w->ds = (int16_t)((w->ds + 256) >> 1);
            } else {
                w->sent_t0 = w->gen_t;
                add_cmd(w, F0_PHRASE, w->gen_t - 200, 0, rmul(A_PHRASE, r));
                w->ds = 256;
            }
        }
    }
    w->gi++;
}

void fx_gen_fill(CtFxWork* w, int32_t t_play)
{
    uint8_t i, j;
    while (w->stage < 2 && w->gen_t < t_play + CTFX_LOOKAHEAD &&
           w->seg_end - w->seg_cur < CTFX_SEGQ - 8) {
        if (w->stage == 0) {
            add_seg(w, S_SIL, 0, w->koe.mora[0].vowel, 0, LEAD_MS, 0);
            w->sent_t0 = w->gen_t;
            add_cmd(w, F0_PHRASE, w->gen_t - 200, 0, rmul(A_PHRASE, w->r));
            w->ds = 256;
            w->stage = 1;
        } else if (w->gi < (int16_t)w->koe.n_mora) {
            gen_mora(w);
        } else {
            w->stage = 2;
            w->total_ms = w->gen_t;
        }
    }
    /* 役目を終えた指令を捨てる */
    for (i = 0, j = 0; i < w->n_cmd; i++) {
        const CtFxCmd* c = &w->cmd[i];
        uint8_t keep = c->kind == F0_PHRASE ? t_play - c->t0 < 4000 : t_play - c->t1 < 500;
        if ((int8_t)i == w->open_acc) { keep = 1; w->open_acc = (int8_t)j; }
        if (keep) w->cmd[j++] = *c;
    }
    w->n_cmd = j;
}
