/* カナ記号列パーサのテスト */
#include <stdio.h>
#include <string.h>
#include "ct_koe.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static CtMora g_mora[64];
static CtPhrase g_phrase[16];
static CtKoe K;

static int parse(const char* s, int* pos)
{
    K.mora = g_mora; K.max_mora = 64;
    K.phrase = g_phrase; K.max_phrase = 16;
    return ct_koe_parse(s, &K, pos);
}

static void expect_error(const char* s, int err, int pos)
{
    int p, e = parse(s, &p);
    if (e != err || p != pos) {
        printf("FAIL \"%s\": got err=%d pos=%d, want err=%d pos=%d\n", s, e, p, err, pos);
        failures++;
    }
}

int main(void)
{
    int pos;

    CHECK(parse("こんにちわ", &pos) == CT_OK);
    CHECK(K.n_mora == 5);
    CHECK(K.mora[0].cons == C_K && K.mora[0].vowel == V_O);
    CHECK(K.mora[1].type == M_N);
    CHECK(K.mora[3].cons == C_CH && K.mora[3].vowel == V_I);
    CHECK(K.mora[4].glide == G_W && K.mora[4].vowel == V_A);
    CHECK(K.n_phrase == 1 && K.phrase[0].accent == 0 && K.phrase[0].end == PE_PERIOD);

    CHECK(parse("キャット", &pos) == CT_OK);
    CHECK(K.n_mora == 3 && K.mora[0].cons == C_K && K.mora[0].glide == G_Y && K.mora[1].type == M_Q);

    CHECK(parse("ふぁいてぃんぐ", &pos) == CT_OK);
    CHECK(K.n_mora == 5 && K.mora[0].cons == C_F && K.mora[2].cons == C_T && K.mora[2].vowel == V_I);

    CHECK(parse("らーめん", &pos) == CT_OK);
    CHECK(K.mora[1].type == M_LONG && K.mora[1].vowel == V_A);

    CHECK(parse("は'し", &pos) == CT_OK && K.phrase[0].accent == 1);
    CHECK(parse("はし'", &pos) == CT_OK && K.phrase[0].accent == 2);

    CHECK(parse("あ/い、う。え？", &pos) == CT_OK);
    CHECK(K.n_phrase == 4 && K.phrase[0].end == PE_NONE && K.phrase[1].end == PE_COMMA &&
          K.phrase[2].end == PE_PERIOD && K.phrase[3].end == PE_QUESTION && K.mora[3].phrase == 3);

    CHECK(parse("きょ＇うわ　いい／てんき．", &pos) == CT_OK);
    CHECK(K.n_phrase == 2 && K.phrase[0].accent == 1 && K.phrase[0].count == 5);

    CHECK(parse("です。", &pos) == CT_OK && K.mora[1].devoice == 1);
    CHECK(parse("です？", &pos) == CT_OK && K.mora[1].devoice == 0);
    CHECK(parse("きた", &pos) == CT_OK && K.mora[0].devoice == 1);
    CHECK(parse("き'た", &pos) == CT_OK && K.mora[0].devoice == 0);
    CHECK(parse("_あ", &pos) == CT_OK && K.mora[0].devoice == 1 && K.mora[0].forced == 1);

    expect_error("", CT_ERR_EMPTY, 0);
    expect_error("、。", CT_ERR_EMPTY, 0);
    expect_error("'あ", CT_ERR_ACCENT, 0);
    expect_error("あ'い'", CT_ERR_ACCENT, 3);
    expect_error("あxい", CT_ERR_CHAR, 1);
    expect_error("あ漢", CT_ERR_CHAR, 1);
    expect_error("ーあ", CT_ERR_LONG, 0);
    expect_error("あっー", CT_ERR_LONG, 2);
    expect_error("あ_", CT_ERR_DEVOICE, 1);
    expect_error("_ん", CT_ERR_DEVOICE, 0);
    expect_error("あ\xff", CT_ERR_UTF8, 1);
    expect_error("きゃきゃ漢", CT_ERR_CHAR, 4);

    /* 長さの上限 */
    {
        char buf[3 * 70 + 1];
        int i;
        for (i = 0; i < 70; i++) memcpy(buf + 3 * i, "あ", 3);
        buf[3 * 70] = 0;
        CHECK(parse(buf, &pos) == CT_ERR_TOOLONG && pos == 64);
    }

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("koe: all tests passed\n");
    return 0;
}
