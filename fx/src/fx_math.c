/* 固定小数点コア: 表を引く補助関数  SPDX-License-Identifier: MIT */
#include "fx_internal.h"

int16_t fx_db(int16_t db)
{
    if (db <= 0) return 0;
    if (db > 72) db = 72;
    return FX_DB[db];
}

/* 表 t[0..256] を位置 pos（Q8, 0..65536）で線形補間 */
static int16_t lerp257(const int16_t* t, int32_t pos)
{
    int16_t i;
    int32_t fr, a;
    if (pos <= 0) return t[0];
    if (pos >= 65536L) return t[256];
    i = (int16_t)(pos >> 8);
    fr = pos & 255;
    a = t[i];
    return (int16_t)(a + (((int32_t)t[i + 1] - a) * fr >> 8));
}

int16_t fx_cos_hz(int16_t hz)
{
    /* 角度 2πf/fs を π 単位にすると 2f/fs。0..fs/2 を 0..65536 に */
    return lerp257(FX_COS, (int32_t)hz * 65536L / (CTFX_FS / 2));
}

int16_t fx_exp_bw(int16_t bw)
{
    /* 表は 16Hz 刻みで 0..4096Hz */
    return lerp257(FX_EXPBW, (int32_t)bw * 16);
}

uint16_t fx_speed_scale(const uint16_t* t, int16_t speed)
{
    int16_t i, fr;
    if (speed <= 50) return t[0];
    if (speed >= 300) return t[25];
    i = (int16_t)((speed - 50) / 10);
    fr = (int16_t)((speed - 50) % 10);
    return (uint16_t)(t[i] + ((int32_t)((int32_t)t[i + 1] - t[i]) * fr) / 10);
}

int16_t fx_gp(int32_t t)
{
    int32_t i, fr;
    if (t <= 0) return 0;
    if (t >= 255L * 16) return FX_GP[255];
    i = t >> 4;
    fr = t & 15;
    return (int16_t)(FX_GP[i] + (((int32_t)FX_GP[i + 1] - FX_GP[i]) * fr >> 4));
}

int16_t fx_ga(int32_t t)
{
    int32_t i, fr;
    if (t <= 0) return 0;
    if (t >= 400) return FX_GA[100];
    i = t >> 2;
    fr = t & 3;
    return (int16_t)(FX_GA[i] + (((int32_t)FX_GA[i + 1] - FX_GA[i]) * fr >> 2));
}

uint16_t fx_hz_from_log2(uint16_t base, int16_t x)
{
    int16_t oct = (int16_t)(x >> 10);        /* 算術シフト（負は切り下げ） */
    int16_t fr = (int16_t)(x & 1023);        /* 0..1023 */
    int16_t i = (int16_t)(fr >> 2), f2 = (int16_t)(fr & 3);
    uint32_t m = FX_EXP2[i] + (((uint32_t)(FX_EXP2[i + 1] - FX_EXP2[i]) * (uint32_t)f2) >> 2);
    uint32_t hz = (uint32_t)base * m;         /* Q14 */
    if (oct >= 0) hz <<= oct; else hz >>= -oct;
    hz >>= 14;
    return (uint16_t)(hz > 2000 ? 2000 : hz < 30 ? 30 : hz);
}

uint16_t fx_isqrt32(uint32_t x)
{
    uint32_t r = 0, bit = 1UL << 30;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (uint16_t)r;
}

int16_t fx_smooth256(int32_t num, int32_t den)
{
    int32_t x;
    if (den <= 0 || num >= den) return 256;
    if (num <= 0) return 0;
    x = num * 256 / den;                     /* 0..255 */
    return (int16_t)((x * x * (768 - 2 * x)) >> 16);
}
