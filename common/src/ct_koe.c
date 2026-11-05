/*
 * カナ記号列のパーサ（V1 / V2 / V3 共通）
 * 動的メモリを使わず、呼び出し側が渡した配列にモーラとアクセント句を書く。
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "ct_koe.h"

#define CP_LONG 0x30FCu  /* ー */
#define CP_Q    0x3063u  /* っ */
#define CP_N    0x3093u  /* ん */

typedef struct { char k[7]; uint8_t c, g, v; } KanaEnt;

#define A V_A
#define I V_I
#define U V_U
#define E V_E
#define O V_O

static const KanaEnt TABLE[] = {
    {"あ",0,0,A},{"い",0,0,I},{"う",0,0,U},{"え",0,0,E},{"お",0,0,O},
    {"ぁ",0,0,A},{"ぃ",0,0,I},{"ぅ",0,0,U},{"ぇ",0,0,E},{"ぉ",0,0,O},
    {"を",0,0,O},{"ゐ",0,0,I},{"ゑ",0,0,E},
    {"か",C_K,0,A},{"き",C_K,0,I},{"く",C_K,0,U},{"け",C_K,0,E},{"こ",C_K,0,O},
    {"ゕ",C_K,0,A},{"ゖ",C_K,0,E},
    {"が",C_G,0,A},{"ぎ",C_G,0,I},{"ぐ",C_G,0,U},{"げ",C_G,0,E},{"ご",C_G,0,O},
    {"さ",C_S,0,A},{"し",C_SH,0,I},{"す",C_S,0,U},{"せ",C_S,0,E},{"そ",C_S,0,O},
    {"ざ",C_Z,0,A},{"じ",C_J,0,I},{"ず",C_Z,0,U},{"ぜ",C_Z,0,E},{"ぞ",C_Z,0,O},
    {"た",C_T,0,A},{"ち",C_CH,0,I},{"つ",C_TS,0,U},{"て",C_T,0,E},{"と",C_T,0,O},
    {"だ",C_D,0,A},{"ぢ",C_J,0,I},{"づ",C_Z,0,U},{"で",C_D,0,E},{"ど",C_D,0,O},
    {"な",C_N,0,A},{"に",C_N,0,I},{"ぬ",C_N,0,U},{"ね",C_N,0,E},{"の",C_N,0,O},
    {"は",C_H,0,A},{"ひ",C_HY,0,I},{"ふ",C_F,0,U},{"へ",C_H,0,E},{"ほ",C_H,0,O},
    {"ば",C_B,0,A},{"び",C_B,0,I},{"ぶ",C_B,0,U},{"べ",C_B,0,E},{"ぼ",C_B,0,O},
    {"ぱ",C_P,0,A},{"ぴ",C_P,0,I},{"ぷ",C_P,0,U},{"ぺ",C_P,0,E},{"ぽ",C_P,0,O},
    {"ま",C_M,0,A},{"み",C_M,0,I},{"む",C_M,0,U},{"め",C_M,0,E},{"も",C_M,0,O},
    {"や",0,G_Y,A},{"ゆ",0,G_Y,U},{"よ",0,G_Y,O},{"ゃ",0,G_Y,A},{"ゅ",0,G_Y,U},{"ょ",0,G_Y,O},
    {"わ",0,G_W,A},{"ゎ",0,G_W,A},
    {"ら",C_R,0,A},{"り",C_R,0,I},{"る",C_R,0,U},{"れ",C_R,0,E},{"ろ",C_R,0,O},
    {"ゔ",C_B,0,U},
    /* 拗音 */
    {"きゃ",C_K,G_Y,A},{"きゅ",C_K,G_Y,U},{"きょ",C_K,G_Y,O},{"きぇ",C_K,G_Y,E},
    {"ぎゃ",C_G,G_Y,A},{"ぎゅ",C_G,G_Y,U},{"ぎょ",C_G,G_Y,O},{"ぎぇ",C_G,G_Y,E},
    {"しゃ",C_SH,0,A},{"しゅ",C_SH,0,U},{"しょ",C_SH,0,O},{"しぇ",C_SH,0,E},
    {"じゃ",C_J,0,A},{"じゅ",C_J,0,U},{"じょ",C_J,0,O},{"じぇ",C_J,0,E},
    {"ちゃ",C_CH,0,A},{"ちゅ",C_CH,0,U},{"ちょ",C_CH,0,O},{"ちぇ",C_CH,0,E},
    {"ぢゃ",C_J,0,A},{"ぢゅ",C_J,0,U},{"ぢょ",C_J,0,O},{"ぢぇ",C_J,0,E},
    {"にゃ",C_N,G_Y,A},{"にゅ",C_N,G_Y,U},{"にょ",C_N,G_Y,O},{"にぇ",C_N,G_Y,E},
    {"ひゃ",C_HY,0,A},{"ひゅ",C_HY,0,U},{"ひょ",C_HY,0,O},{"ひぇ",C_HY,0,E},
    {"びゃ",C_B,G_Y,A},{"びゅ",C_B,G_Y,U},{"びょ",C_B,G_Y,O},{"びぇ",C_B,G_Y,E},
    {"ぴゃ",C_P,G_Y,A},{"ぴゅ",C_P,G_Y,U},{"ぴょ",C_P,G_Y,O},{"ぴぇ",C_P,G_Y,E},
    {"みゃ",C_M,G_Y,A},{"みゅ",C_M,G_Y,U},{"みょ",C_M,G_Y,O},{"みぇ",C_M,G_Y,E},
    {"りゃ",C_R,G_Y,A},{"りゅ",C_R,G_Y,U},{"りょ",C_R,G_Y,O},{"りぇ",C_R,G_Y,E},
    /* 外来音 */
    {"ふぁ",C_F,0,A},{"ふぃ",C_F,0,I},{"ふぇ",C_F,0,E},{"ふぉ",C_F,0,O},{"ふゅ",C_F,G_Y,U},
    {"てぃ",C_T,0,I},{"でぃ",C_D,0,I},{"とぅ",C_T,0,U},{"どぅ",C_D,0,U},
    {"てゅ",C_T,G_Y,U},{"でゅ",C_D,G_Y,U},{"すぃ",C_S,0,I},{"ずぃ",C_Z,0,I},
    {"つぁ",C_TS,0,A},{"つぃ",C_TS,0,I},{"つぇ",C_TS,0,E},{"つぉ",C_TS,0,O},
    {"うぃ",0,G_W,I},{"うぇ",0,G_W,E},{"うぉ",0,G_W,O},{"いぇ",0,G_Y,E},
    {"くぁ",C_K,G_W,A},{"くぃ",C_K,G_W,I},{"くぇ",C_K,G_W,E},{"くぉ",C_K,G_W,O},{"ぐぁ",C_G,G_W,A},
    {"ゔぁ",C_B,0,A},{"ゔぃ",C_B,0,I},{"ゔぇ",C_B,0,E},{"ゔぉ",C_B,0,O},{"ゔゅ",C_B,G_Y,U},
};

#undef A
#undef I
#undef U
#undef E
#undef O

/* manner | place << 3 | voiced << 6 */
#define CI(m, p, v) (uint8_t)((m) | ((p) << 3) | ((v) << 6))
static const uint8_t CONS[C_COUNT] = {
    CI(MN_NONE,  PL_NONE, 1), /* none */
    CI(MN_STOP,  PL_VEL,  0), /* k  */
    CI(MN_STOP,  PL_VEL,  1), /* g  */
    CI(MN_FRIC,  PL_ALV,  0), /* s  */
    CI(MN_FRIC,  PL_PAL,  0), /* sh */
    CI(MN_AFFR,  PL_ALV,  1), /* z  */
    CI(MN_AFFR,  PL_PAL,  1), /* j  */
    CI(MN_STOP,  PL_ALV,  0), /* t  */
    CI(MN_STOP,  PL_ALV,  1), /* d  */
    CI(MN_AFFR,  PL_ALV,  0), /* ts */
    CI(MN_AFFR,  PL_PAL,  0), /* ch */
    CI(MN_NASAL, PL_ALV,  1), /* n  */
    CI(MN_FRIC,  PL_GLOT, 0), /* h  */
    CI(MN_FRIC,  PL_PAL,  0), /* hy */
    CI(MN_FRIC,  PL_LAB,  0), /* f  */
    CI(MN_STOP,  PL_LAB,  1), /* b  */
    CI(MN_STOP,  PL_LAB,  0), /* p  */
    CI(MN_NASAL, PL_LAB,  1), /* m  */
    CI(MN_FLAP,  PL_ALV,  1), /* r  */
};

uint8_t ct_cons_info(int cons)
{
    return CONS[cons >= 0 && cons < C_COUNT ? cons : 0];
}

/* UTF-8 を 1 文字読む。不正なら 0xFFFFFFFF */
static uint32_t next_cp(const unsigned char** pp)
{
    const unsigned char* p = *pp;
    uint32_t c;
    int extra, i;
    if (*p < 0x80)                { c = *p;        extra = 0; }
    else if ((*p & 0xE0) == 0xC0) { c = *p & 0x1F; extra = 1; }
    else if ((*p & 0xF0) == 0xE0) { c = *p & 0x0F; extra = 2; }
    else if ((*p & 0xF8) == 0xF0) { c = *p & 0x07; extra = 3; }
    else return 0xFFFFFFFFu;
    p++;
    for (i = 0; i < extra; i++, p++) {
        if ((*p & 0xC0) != 0x80) return 0xFFFFFFFFu;
        c = (c << 6) | (*p & 0x3F);
    }
    if ((extra == 1 && c < 0x80) || (extra == 2 && c < 0x800) ||
        (extra == 3 && (c < 0x10000 || c > 0x10FFFF)) || (c >= 0xD800 && c <= 0xDFFF))
        return 0xFFFFFFFFu;
    *pp = p;
    return c;
}

static uint32_t normalize(uint32_t c)
{
    if (c >= 0x30A1 && c <= 0x30F6) return c - 0x60;   /* カタカナ → ひらがな */
    switch (c) {
    case 0x2019: case 0xFF07:              return '\'';
    case 0xFF0F:                           return '/';
    case 0x3001: case 0xFF0C:              return ',';
    case 0x3002: case 0xFF0E:              return '.';
    case 0xFF01:                           return '!';
    case 0xFF1F:                           return '?';
    case 0xFF3F:                           return '_';
    case 0x301C: case 0xFF5E: case 0x2015: return CP_LONG;
    default:                               return c;
    }
}

static int is_space(uint32_t c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0x3000;
}

/* 表の項目の n 文字目のコードポイント（表はすべて 3 バイト文字） */
static uint32_t ent_cp(const KanaEnt* e, int n)
{
    const unsigned char* p = (const unsigned char*)e->k + 3 * n;
    if (!p[0]) return 0;
    return ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
}

/* c0（と c1）に当たる最長の項目。戻り値は使った文字数 */
static int lookup(uint32_t c0, uint32_t c1, uint8_t* cons, uint8_t* glide, uint8_t* vowel)
{
    unsigned i;
    int best = 0;
    for (i = 0; i < sizeof TABLE / sizeof TABLE[0]; i++) {
        const KanaEnt* e = &TABLE[i];
        int len;
        if (ent_cp(e, 0) != c0) continue;
        len = ent_cp(e, 1) ? 2 : 1;
        if (len == 2 && ent_cp(e, 1) != c1) continue;
        if (len <= best) continue;
        best = len;
        *cons = e->c;
        *glide = e->g;
        *vowel = e->v;
    }
    return best;
}

static int voiceless(int c)
{
    return c == C_K || c == C_S || c == C_SH || c == C_T || c == C_CH || c == C_TS ||
           c == C_H || c == C_HY || c == C_F || c == C_P;
}

/* 無声子音に挟まれた i/u、文末の「す」「つ」を無声化する */
static void auto_devoice(CtKoe* k)
{
    int i;
    for (i = 0; i < (int)k->n_mora; i++) {
        CtMora* m = &k->mora[i];
        const CtPhrase* ph = &k->phrase[m->phrase];
        int idx = i - ph->first;
        int last = idx == (int)ph->count - 1;
        if (m->type != M_NORMAL || m->devoice) continue;
        if (m->vowel != V_I && m->vowel != V_U) continue;
        if (!voiceless(m->cons) || m->glide != G_NONE) continue;
        if (i > 0 && k->mora[i - 1].devoice) continue;
        if (ph->accent == idx + 1) continue;
        if (!last || ph->end == PE_NONE) {
            if (i + 1 < (int)k->n_mora) {
                const CtMora* nx = &k->mora[i + 1];
                if ((nx->type == M_NORMAL && voiceless(nx->cons)) || nx->type == M_Q)
                    m->devoice = 1;
            }
        } else if (ph->end == PE_PERIOD && m->vowel == V_U && (m->cons == C_S || m->cons == C_TS)) {
            m->devoice = 1;
        }
    }
}

static int push_phrase(CtKoe* k, int first, int count, int accent, int end)
{
    CtPhrase* ph;
    if (k->n_phrase >= k->max_phrase) return CT_ERR_TOOLONG;
    ph = &k->phrase[k->n_phrase++];
    ph->first = (uint16_t)first;
    ph->count = (uint16_t)count;
    ph->accent = (uint8_t)(accent > 255 ? 255 : accent);
    ph->end = (uint8_t)end;
    return CT_OK;
}

int ct_koe_parse(const char* text, CtKoe* k, int* err_pos)
{
    const unsigned char* p = (const unsigned char*)text;
    int pos = 0, err = CT_OK;
    int ph_first = 0, ph_accent = 0;
    int dev_pending = 0, dev_pos = -1;

    *err_pos = -1;
    if (!text || !k || !k->mora || !k->phrase) return CT_ERR_NULL;
    k->n_mora = 0;
    k->n_phrase = 0;

    while (*p) {
        const unsigned char* q = p;
        uint32_t c = next_cp(&q), c1 = 0;
        int cur = k->n_mora - ph_first, used = 1;
        CtMora m;

        if (c == 0xFFFFFFFFu) { *err_pos = pos; return CT_ERR_UTF8; }
        c = normalize(c);
        if (*q) {
            const unsigned char* r = q;
            c1 = next_cp(&r);
            if (c1 != 0xFFFFFFFFu) c1 = normalize(c1);
        }

        if (is_space(c)) { p = q; pos++; continue; }
        if (c == '\'') {
            if (cur == 0 || ph_accent) { *err_pos = pos; return CT_ERR_ACCENT; }
            ph_accent = cur;
            p = q; pos++;
            continue;
        }
        if (c == '/' || c == ',' || c == '.' || c == '!' || c == '?') {
            int end = c == '/' ? PE_NONE : c == ',' ? PE_COMMA : c == '?' ? PE_QUESTION : PE_PERIOD;
            if (dev_pending) { *err_pos = dev_pos; return CT_ERR_DEVOICE; }
            if (cur > 0) {
                if ((err = push_phrase(k, ph_first, cur, ph_accent, end)) != CT_OK) { *err_pos = pos; return err; }
                ph_first = k->n_mora;
                ph_accent = 0;
            } else if (k->n_phrase > 0 && end > k->phrase[k->n_phrase - 1].end) {
                k->phrase[k->n_phrase - 1].end = (uint8_t)end;
            }
            p = q; pos++;
            continue;
        }
        if (c == '_') {
            if (dev_pending) { *err_pos = pos; return CT_ERR_DEVOICE; }
            dev_pending = 1;
            dev_pos = pos;
            p = q; pos++;
            continue;
        }

        memset(&m, 0, sizeof m);
        m.phrase = k->n_phrase;
        if (c == CP_LONG) {
            const CtMora* pv;
            if (cur == 0) { *err_pos = pos; return CT_ERR_LONG; }
            pv = &k->mora[k->n_mora - 1];
            if (pv->type == M_Q) { *err_pos = pos; return CT_ERR_LONG; }
            m.type = pv->type == M_N ? M_N : M_LONG;
            m.vowel = pv->vowel;
        } else if (c == CP_Q) {
            m.type = M_Q;
        } else if (c == CP_N) {
            m.type = M_N;
        } else {
            used = lookup(c, c1, &m.cons, &m.glide, &m.vowel);
            if (!used) { *err_pos = pos; return CT_ERR_CHAR; }
            m.type = M_NORMAL;
        }
        if (dev_pending) {
            if (m.type != M_NORMAL) { *err_pos = dev_pos; return CT_ERR_DEVOICE; }
            m.devoice = 1;
            m.forced = 1;
            dev_pending = 0;
        }
        if (k->n_mora >= k->max_mora) { *err_pos = pos; return CT_ERR_TOOLONG; }
        k->mora[k->n_mora++] = m;
        p = q;
        pos++;
        if (used == 2) { next_cp(&p); pos++; }
    }

    if (dev_pending) { *err_pos = dev_pos; return CT_ERR_DEVOICE; }
    if (k->n_mora - ph_first > 0) {
        if ((err = push_phrase(k, ph_first, k->n_mora - ph_first, ph_accent, PE_PERIOD)) != CT_OK) {
            *err_pos = pos;
            return err;
        }
    } else if (k->n_phrase > 0 && k->phrase[k->n_phrase - 1].end == PE_NONE) {
        k->phrase[k->n_phrase - 1].end = PE_PERIOD;
    }
    if (k->n_mora == 0) { *err_pos = 0; return CT_ERR_EMPTY; }
    auto_devoice(k);
    return CT_OK;
}
