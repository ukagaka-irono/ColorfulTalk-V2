/*
 * fxgen: 固定小数点コアの表（fx/src/fx_tables.c）を作る（V2 の声は tools/mkvoice.c が作る）。
 * PC 上で一度だけ動かす開発用ツール（浮動小数点を使う）。生成物はリポジトリに入れる。
 *   fxgen <fx_tables.c の出力先>
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI 3.14159265358979323846
#define FS 8000.0

static long rnd(double x) { return (long)floor(x + 0.5); }

/* ---- 表 ------------------------------------------------------------------ */
static void row16(FILE* f, const char* decl, const long* v, int n)
{
    int i;
    fprintf(f, "%s[%d] = {", decl, n);
    for (i = 0; i < n; i++) fprintf(f, "%s%ld%s", i % 12 ? " " : "\n    ", v[i], i < n - 1 ? "," : "");
    fprintf(f, "\n};\n\n");
}

static void gen_tables(FILE* f)
{
    long v[300];
    int i;
    fprintf(f, "/* 生成ファイル（tools/fxgen.c）。手で編集しないこと。  SPDX-License-Identifier: MIT */\n");
    fprintf(f, "#include \"fx_internal.h\"\n\n");

    for (i = 0; i <= 256; i++) v[i] = rnd(16384.0 * cos(PI * i / 256.0));
    fprintf(f, "/* cos(pi*i/256)  Q14 */\n");
    row16(f, "const int16_t FX_COS", v, 257);

    for (i = 0; i <= 256; i++) v[i] = rnd(16384.0 * exp(-PI * (i * 16.0) / FS));
    fprintf(f, "/* exp(-pi*bw/fs), bw = i*16 Hz  Q14 */\n");
    row16(f, "const int16_t FX_EXPBW", v, 257);

    for (i = 0; i <= 72; i++) v[i] = i == 0 ? 0 : rnd(4096.0 * pow(10.0, (i - 60) / 20.0));
    fprintf(f, "/* 線形振幅（60dB = 4096）  Q12 */\n");
    row16(f, "const int16_t FX_DB", v, 73);

    for (i = 0; i <= 256; i++) v[i] = rnd(16384.0 * pow(2.0, i / 256.0));
    fprintf(f, "/* 2^(i/256)  Q14 */\n");
    row16(f, "const uint16_t FX_EXP2", v, 257);

    for (i = 0; i < 256; i++) { double t = i * 0.016; v[i] = rnd(4096.0 * 9.0 * t * exp(-3.0 * t)); }
    fprintf(f, "/* 藤崎モデルのフレーズ成分 Gp(t), t = i*16ms, alpha = 3  Q12 */\n");
    row16(f, "const int16_t FX_GP", v, 256);

    for (i = 0; i <= 100; i++) {
        double t = i * 0.004, g = 1.0 - (1.0 + 20.0 * t) * exp(-20.0 * t);
        v[i] = rnd(4096.0 * (g < 0.9 ? g : 0.9));
    }
    fprintf(f, "/* 藤崎モデルのアクセント成分 Ga(t), t = i*4ms, beta = 20, gamma = 0.9  Q12 */\n");
    row16(f, "const int16_t FX_GA", v, 101);

    for (i = 0; i < 64; i++) v[i] = rnd(16384.0 * sin(2.0 * PI * i / 64.0));
    fprintf(f, "/* sin(2*pi*i/64)  Q14 */\n");
    row16(f, "const int16_t FX_SIN", v, 64);

    for (i = 0; i <= 25; i++) v[i] = rnd(256.0 * pow(100.0 / (50 + 10 * i), 0.7));
    fprintf(f, "/* 子音の長さの倍率 (100/speed)^0.7, speed = 50 + 10*i  Q8 */\n");
    row16(f, "const uint16_t FX_CSC", v, 26);
    for (i = 0; i <= 25; i++) v[i] = rnd(256.0 * pow(100.0 / (50 + 10 * i), 1.15));
    fprintf(f, "/* 母音の長さの倍率 (100/speed)^1.15  Q8 */\n");
    row16(f, "const uint16_t FX_VSC", v, 26);
}

int main(int argc, char** argv)
{
    FILE* a;
    if (argc != 2) { fprintf(stderr, "usage: fxgen fx_tables.c\n"); return 2; }
    a = fopen(argv[1], "wb");
    if (!a) { fprintf(stderr, "fxgen: cannot write\n"); return 1; }
    gen_tables(a);
    fclose(a);
    return 0;
}
