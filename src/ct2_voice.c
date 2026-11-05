/* ColorfulTalk V2: 声データの検査と読み込み  SPDX-License-Identifier: MIT */
#include <string.h>
#include "colorful_talk_v2.h"
#include "ct2_voice.h"

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t rd32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* CRC-32（IEEE 802.3）。表を持たない小さな実装。skip_at から 4 バイトは 0 とみなす */
uint32_t ct2_crc32(const uint8_t* p, size_t n, size_t skip_at)
{
    uint32_t crc = 0xFFFFFFFFUL;
    size_t i;
    uint8_t k;
    for (i = 0; i < n; i++) {
        uint8_t b = (i >= skip_at && i < skip_at + 4) ? 0 : p[i];
        crc ^= b;
        for (k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
    return ~crc;
}

int ct2_voice_load(const void* voice, size_t size, CtFxVoice* out)
{
    const uint8_t* p = (const uint8_t*)voice;
    uint32_t total;
    uint16_t ob, op, i, n;
    const uint8_t* b;

    if (!p) return CT_ERR_NULL;
    if (size != 0 && size < CTV_HEADER) return CT_ERR_VOICE;
    if (p[0] != CTV_MAGIC0 || p[1] != CTV_MAGIC1 || p[2] != CTV_MAGIC2 || p[3] != CTV_MAGIC3) return CT_ERR_VOICE;
    if (rd16(p + 4) != CTV_FORMAT || rd16(p + 6) != CTV_HEADER) return CT_ERR_VOICE;
    total = rd32(p + 8);
    if (total < CTV_HEADER || total > CT2_VOICE_MAX_SIZE) return CT_ERR_VOICE;
    if (size != 0 && total > size) return CT_ERR_VOICE;
    if (ct2_crc32(p, total, 12) != rd32(p + 12)) return CT_ERR_VOICE;

    ob = rd16(p + 32);
    op = rd16(p + 34);
    if (ob < CTV_HEADER || (uint32_t)ob + CTV_BASIC_SIZE > total) return CT_ERR_VOICE;
    b = p + ob;
    n = b[13];
    if (n != CTFX_PULSE_N) return CT_ERR_VOICE;
    if (op < CTV_HEADER || (uint32_t)op + 2u * n > total) return CT_ERR_VOICE;

    /* 値の範囲（合成器が壊れない範囲）も確かめる */
    {
        uint16_t f0 = rd16(b + 0), fscale = rd16(b + 4);
        if (f0 < 40 || f0 > 600 || fscale < 2048 || fscale > 8192 || b[12] == 0 || b[12] > 63 || b[3] > 1)
            return CT_ERR_VOICE;
        if (!out) return CT_OK;
        memset(out, 0, sizeof *out);
        for (i = 0; i < sizeof out->name - 1 && p[16 + i]; i++) out->name[i] = (char)p[16 + i];
        out->f0 = f0;
        out->f0range = b[2];
        out->whisper = b[3];
        out->fscale = fscale;
        out->b1add = b[6];
        out->tilt = b[7];
        out->breath_db = b[8];
        out->jitter = b[9];
        out->gain = rd16(b + 10);
        out->te = b[12];
        for (i = 0; i < n; i++) out->pulse[i] = (int16_t)rd16(p + op + 2 * i);
    }
    return CT_OK;
}
