/* ColorfulTalk V2 のテスト   test_v2 <voices フォルダ> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "colorful_talk_v2.h"
#include "ct2_voice.h"
#include "ct_test_audio.h"

extern const unsigned char* const CT2_BUILTIN_VOICES[];
extern const char* const CT2_BUILTIN_NAMES[];
extern const int CT2_BUILTIN_COUNT;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define FS CT2_SAMPLE_RATE

static const char* ALL_MORA[] = {
    "あ","い","う","え","お","か","き","く","け","こ","が","ぎ","ぐ","げ","ご",
    "さ","し","す","せ","そ","ざ","じ","ず","ぜ","ぞ","た","ち","つ","て","と",
    "だ","ぢ","づ","で","ど","な","に","ぬ","ね","の","は","ひ","ふ","へ","ほ",
    "ば","び","ぶ","べ","ぼ","ぱ","ぴ","ぷ","ぺ","ぽ","ま","み","む","め","も",
    "や","ゆ","よ","ら","り","る","れ","ろ","わ","を","ん","ゔ",
    "きゃ","きゅ","きょ","しゃ","ちゃ","にゃ","ひゃ","みゃ","りゃ","ふぁ","てぃ","うぃ",
    "あっか","あった","あっさ","かーど","さんぽ","さんか","ほんや",
};

static uint8_t* g_work;
static const unsigned char* g_voice;

static size_t voice_size(const unsigned char* v)
{
    return (size_t)v[8] | ((size_t)v[9] << 8) | ((size_t)v[10] << 16) | ((size_t)v[11] << 24);
}

static int synth(const char* koe, int speed, int16_t** out, int* n, int chunk)
{
    int16_t buf[1024];
    int got, cap = 1 << 15, err;
    *n = 0;
    *out = NULL;
    err = ct2_start(g_work, koe, g_voice, speed);
    if (err) return err;
    *out = (int16_t*)malloc((size_t)cap * 2);
    while ((got = ct2_read(g_work, buf, chunk)) > 0) {
        if (*n + got > cap) { cap *= 2; *out = (int16_t*)realloc(*out, (size_t)cap * 2); }
        memcpy(*out + *n, buf, (size_t)got * 2);
        *n += got;
    }
    return CT_OK;
}

static void test_sizes(void)
{
    printf("work size: %u bytes\n", (unsigned)ct2_work_size());
    CHECK(ct2_work_size() <= 4600);
    CHECK(CT2_BUILTIN_COUNT == 14);
}

static void test_voice_data(const char* dir)
{
    unsigned char bad[512];
    size_t n = voice_size(CT2_BUILTIN_VOICES[0]);
    char name[16];
    int k;

    CHECK(n > 64 && n <= CT2_VOICE_MAX_SIZE && n <= sizeof bad);
    CHECK(ct2_voice_check(CT2_BUILTIN_VOICES[0], n) == CT_OK);
    CHECK(ct2_voice_check(CT2_BUILTIN_VOICES[0], n - 1) == CT_ERR_VOICE);   /* 足りない */
    CHECK(ct2_voice_check(CT2_BUILTIN_VOICES[0], 10) == CT_ERR_VOICE);
    CHECK(ct2_voice_check(NULL, n) == CT_ERR_NULL);
    CHECK(ct2_voice_name(CT2_BUILTIN_VOICES[0], name) == CT_OK && strcmp(name, "f1") == 0);

    memcpy(bad, CT2_BUILTIN_VOICES[0], n);
    bad[0] = 'X';                                                          /* マジック */
    CHECK(ct2_voice_check(bad, n) == CT_ERR_VOICE);
    memcpy(bad, CT2_BUILTIN_VOICES[0], n);
    bad[100] ^= 1;                                                         /* 中身が壊れている */
    CHECK(ct2_voice_check(bad, n) == CT_ERR_VOICE);
    CHECK(ct2_start(g_work, "あ", bad, 100) == CT_ERR_VOICE);
    memcpy(bad, CT2_BUILTIN_VOICES[0], n);
    bad[4] = 2;                                                            /* 知らない版 */
    CHECK(ct2_voice_check(bad, n) == CT_ERR_VOICE);

    /* 同梱のファイルと C の配列が同じ */
    for (k = 0; k < CT2_BUILTIN_COUNT; k++) {
        char path[512];
        unsigned char buf[CT2_VOICE_MAX_SIZE];
        size_t got;
        FILE* f;
        sprintf(path, "%s/%s.ctv", dir, CT2_BUILTIN_NAMES[k]);
        f = fopen(path, "rb");
        if (!f) { printf("FAIL cannot open %s\n", path); failures++; continue; }
        got = fread(buf, 1, sizeof buf, f);
        fclose(f);
        CHECK(got == voice_size(CT2_BUILTIN_VOICES[k]) && memcmp(buf, CT2_BUILTIN_VOICES[k], got) == 0);
        CHECK(ct2_voice_check(buf, got) == CT_OK);
    }
}

static void test_every_mora(void)
{
    size_t k;
    g_voice = CT2_BUILTIN_VOICES[0];
    for (k = 0; k < sizeof ALL_MORA / sizeof ALL_MORA[0]; k++) {
        int16_t* x;
        int n, err = synth(ALL_MORA[k], 100, &x, &n, 256);
        double r;
        if (err) { printf("FAIL %s: err %d\n", ALL_MORA[k], err); failures++; continue; }
        r = ta_rms(x, n);
        if (r < 300 || ta_clipped(x, n) > n / 1000) {
            printf("FAIL %s: rms=%.0f clipped=%d/%d\n", ALL_MORA[k], r, ta_clipped(x, n), n);
            failures++;
        }
        free(x);
    }
}

static void test_streaming_and_switch(void)
{
    int16_t *a, *b, *c;
    int na, nb, nc;
    g_voice = CT2_BUILTIN_VOICES[0];
    CHECK(synth("こんにちわ、せか'い。", 100, &a, &na, 1000) == CT_OK);
    CHECK(synth("こんにちわ、せか'い。", 100, &b, &nb, 7) == CT_OK);
    CHECK(na == nb && na > 0 && memcmp(a, b, (size_t)na * 2) == 0);
    g_voice = CT2_BUILTIN_VOICES[5];          /* 声を切り替えると出力が変わる */
    CHECK(synth("こんにちわ、せか'い。", 100, &c, &nc, 256) == CT_OK);
    CHECK(nc != na || memcmp(a, c, (size_t)na * 2) != 0);
    free(a);
    free(b);
    free(c);
    g_voice = CT2_BUILTIN_VOICES[0];
}

static void test_speed_and_pitch(void)
{
    int16_t *a, *b, *x;
    int na, nb, n, s, e, len = FS / 20, w;
    double f1[2], f2[2], fq, fst;
    static const char* words[2] = { "あ'い", "あい'" };

    g_voice = CT2_BUILTIN_VOICES[0];
    CHECK(synth("あいうえお、かきくけこ", 100, &a, &na, 256) == CT_OK);
    CHECK(synth("あいうえお、かきくけこ", 200, &b, &nb, 256) == CT_OK);
    printf("speed x2 length ratio %.2f\n", (double)nb / na);
    CHECK((double)nb / na > 0.45 && (double)nb / na < 0.65);
    free(a);
    free(b);

    g_voice = CT2_BUILTIN_VOICES[5];   /* m1 */
    CHECK(synth("あー", 100, &x, &n, 256) == CT_OK);
    ta_span(x, n, 500, &s, &e);
    f1[0] = ta_f0(x, n, FS, (s + e) / 2 - len / 2, len);
    printf("m1 f0 %.0f Hz\n", f1[0]);
    CHECK(f1[0] > 100 && f1[0] < 160);
    free(x);
    for (w = 0; w < 2; w++) {
        CHECK(synth(words[w], 100, &x, &n, 256) == CT_OK);
        ta_span(x, n, 500, &s, &e);
        f1[w] = ta_f0(x, n, FS, s + (e - s) / 4 - len / 2, len);
        f2[w] = ta_f0(x, n, FS, s + (e - s) * 2 / 3 - len / 2, len);
        free(x);
    }
    printf("accent: あ'い %.0f -> %.0f Hz, あい' %.0f -> %.0f Hz\n", f1[0], f2[0], f1[1], f2[1]);
    CHECK(f1[0] > f2[0] * 1.08);
    CHECK(f2[1] > f1[1] * 1.08);
    CHECK(synth("そうですか？", 100, &x, &n, 256) == CT_OK);
    ta_span(x, n, 500, &s, &e);
    fq = ta_f0_max(x, n, FS, e - FS / 8, e);
    free(x);
    CHECK(synth("そうですか。", 100, &x, &n, 256) == CT_OK);
    ta_span(x, n, 500, &s, &e);
    fst = ta_f0_max(x, n, FS, e - FS / 8, e);
    free(x);
    printf("question end %.0f Hz, statement end %.0f Hz\n", fq, fst);
    CHECK(fq > fst * 1.2);
    g_voice = CT2_BUILTIN_VOICES[0];
}

static void test_voices(void)
{
    int v;
    for (v = 0; v < CT2_BUILTIN_COUNT; v++) {
        int16_t* x;
        int n;
        g_voice = CT2_BUILTIN_VOICES[v];
        CHECK(synth("わたしわ、からふるとーくです。", 100, &x, &n, 256) == CT_OK);
        printf("voice %-3s rms %5.0f clipped %d\n", CT2_BUILTIN_NAMES[v], ta_rms(x, n), ta_clipped(x, n));
        CHECK(ta_rms(x, n) > 1000 && ta_clipped(x, n) < n / 1000);
        free(x);
    }
    g_voice = CT2_BUILTIN_VOICES[0];
}

static void test_errors_and_limits(void)
{
    static char buf[3 * 200 + 8];
    int size = 0, err = 0, i, n;
    int16_t* x;
    g_voice = CT2_BUILTIN_VOICES[0];
    CHECK(ct2_start(g_work, "あいう漢字", g_voice, 100) == CT_ERR_CHAR);
    CHECK(ct2_error_pos(g_work) == 3);
    CHECK(ct2_read(g_work, (int16_t*)&size, 1) == 0);
    CHECK(ct2_start(g_work, "あ", NULL, 100) == CT_ERR_NULL);
    CHECK(ct2_synthe_wav("あ", g_voice, 10, &size, &err) == NULL && err == CT_ERR_PARAM);
    for (i = 0; i < 150; i++) memcpy(buf + 3 * i, i % 20 == 19 ? "、" : "か", 3);
    buf[3 * 150] = 0;
    CHECK(synth(buf, 100, &x, &n, 256) == CT_OK);
    free(x);
    for (i = 0; i < 170; i++) memcpy(buf + 3 * i, "か", 3);
    buf[3 * 170] = 0;
    CHECK(ct2_start(g_work, buf, g_voice, 100) == CT_ERR_TOOLONG);
}

int main(int argc, char** argv)
{
    g_work = (uint8_t*)malloc(ct2_work_size());
    g_voice = CT2_BUILTIN_VOICES[0];
    test_sizes();
    test_voice_data(argc > 1 ? argv[1] : "voices");
    test_every_mora();
    test_streaming_and_switch();
    test_speed_and_pitch();
    test_voices();
    test_errors_and_limits();
    free(g_work);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("v2: all tests passed\n");
    return 0;
}
