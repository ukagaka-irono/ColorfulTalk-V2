# ColorfulTalk V2 仕様書

> シリーズ全体の仕様書: [ColorfulTalk](https://github.com/ukagaka-irono/ColorfulTalk)（`docs/spec/v2.md` と同じ内容）

| 項目 | 内容 |
|---|---|
| 版 | 2.0.0 |
| 位置づけ | 声データを差し替えて、実行中に声を切り替えられるエンジン。後継は V3 |
| 対象 | 16bit 以上の CPU（`int` が 16bit でも動く）、Windows / macOS / Linux |
| 方式 | 規則合成（フォルマント合成）。合成コアは V1 と共通 |
| 入力 | カナ記号列（UTF-8）。[koe-kana.md](koe-kana.md) |
| 出力 | 16bit 符号付き PCM、モノラル、8kHz |
| 演算 | 整数のみ（64bit 演算・浮動小数点は使わない） |
| 作業領域 | 4,168 バイト（`ct2_work_size()`）。呼び出し側が用意する |
| 動的メモリ | ストリーミング API は使わない。`ct2_synthe_wav()` だけ `malloc` を使う |
| 声 | 声データ（`.ctv`、16KB 以下。現在の形式では 212 バイト）をポインタで渡す。14 種を同梱 |
| 記号列の長さ | 1 回に 160 モーラ・40 アクセント句まで |
| ライセンス | MIT |

## 寸法

ATmega1284P（8bit AVR、`int` は 16bit）向けに avr-gcc 16.1 `-Os` でビルドし、シミュレータ（avr8js）で測定。

| 項目 | 値 | 目標 |
|---|---|---|
| ROM（コード＋表） | 約 24KB | 51KB 以下 |
| 声データ | 212 バイト / 声 | 16KB 以下 |
| RAM: 作業領域 | 4,168 バイト | |
| RAM: スタックの最大 | 約 490 バイト | |
| RAM 合計（定数を ROM に置ける CPU） | 約 4.7KB | 5KB 以下 |
| PC 版との出力の一致 | ビット単位で一致（3 種の声 × 3 文で確認） | |
| 処理時間（AVR） | 1 サンプル平均 約 4,400〜4,700 サイクル | |

AVR は定数を RAM にコピーするため、上記に加えて約 4.6KB を使います。ARM・MSP430・RL78 などでは不要です。
処理はフレームの始め（4ms ごと）に集中するため、32 サンプル以上の出力バッファを用意してください。

同じパラメータの声なら、V2 の出力は V1 とビット単位で同じです（f1・m1・w1・e1 で確認）。
明瞭性の評価は V1 と同じです。

## 同梱の声

`voices/` に `.ctv` ファイルとして入っています。一覧は [voices/voices.csv](../voices/voices.csv)。

| ファイル | 声 | F0 | 声道倍率 | Rd |
|---|---|---|---|---|
| `f1.ctv` | 女声（標準） | 210Hz | 1.15 | 1.30 |
| `f2.ctv` | 高めの女声 | 260Hz | 1.22 | 1.20 |
| `f3.ctv` | 落ち着いた女声 | 180Hz | 1.10 | 1.50 |
| `f4.ctv` | 明るい女声 | 230Hz | 1.18 | 1.05 |
| `f5.ctv` | かすれた女声 | 200Hz | 1.12 | 2.20 |
| `m1.ctv` | 男声 | 115Hz | 1.00 | 1.00 |
| `m2.ctv` | 低い男声 | 95Hz | 0.93 | 1.10 |
| `m3.ctv` | 若い男声 | 135Hz | 1.04 | 1.05 |
| `m4.ctv` | 張りのある男声 | 120Hz | 0.98 | 0.70 |
| `c1.ctv` | 子ども | 290Hz | 1.28 | 1.40 |
| `c2.ctv` | 幼い子ども | 330Hz | 1.36 | 1.30 |
| `e1.ctv` | 年配の声 | 150Hz | 1.08 | 1.80 |
| `w1.ctv` | ささやき声 | ― | 1.12 | ― |
| `r1.ctv` | ロボット | 140Hz | 1.05 | 0.50 |

声データの形式は [voice-data.md](voice-data.md) を参照してください。`tools/mkvoice` で自分の声も作れます。

## API

ヘッダ: [include/colorful_talk_v2.h](../include/colorful_talk_v2.h)

```c
int    ct2_voice_check(const void* voice, size_t size);   /* 声データの検査 */
int    ct2_voice_name(const void* voice, char* name);     /* 声の名前（name は 16 バイト以上） */

size_t ct2_work_size(void);
int    ct2_start(void* work, const char* koe_utf8, const void* voice, int speed);  /* speed 50〜300 */
int    ct2_read(void* work, int16_t* buf, int n);                                  /* 0 で終わり */
int    ct2_error_pos(const void* work);

/* PC 向け（CT_NO_WAV を定義すると外れる） */
unsigned char* ct2_synthe_wav(const char* koe_utf8, const void* voice, int speed, int* size, int* err);
void           ct2_free(void* buf);
```

| 関数 | 説明 |
|---|---|
| `ct2_voice_check` | 声データの形式・版・大きさ・CRC・値の範囲を確かめる。正しければ `CT_OK`、不正なら `CT_ERR_VOICE` |
| `ct2_voice_name` | 声データに入っている声の名前 |
| `ct2_start` | 声データを検査して作業領域に写し、記号列を解析して合成を始める。声データは `ct2_start` の後は捨ててよい |
| `ct2_read` | 最大 `n` サンプルを書き、書いた数を返す。0 なら終わり。何サンプルずつ読んでも結果は同じ |
| `ct2_error_pos` | `ct2_start` が記号列のエラーで失敗したとき、原因の文字位置。不明なら -1 |

作業領域は 4 バイト境界に置いてください。

```c
extern const unsigned char voice_f1[];   /* f1.ctv を ROM に置いたもの */
static uint8_t work[4352];
int16_t buf[64];
int n;

if (ct2_start(work, "こんにちわ、せか'い。", voice_f1, 100) == CT_OK) {
    while ((n = ct2_read(work, buf, 64)) > 0) {
        /* buf[0..n) を DAC へ（8kHz） */
    }
}
```

声を切り替えるときは、次の `ct2_start` に別の声データを渡すだけです。

## エラーコード

[errors.md](errors.md) を参照してください。V2 が返すのは `CT_OK`、`CT_ERR_NULL`、記号列のエラー（2〜7, 9）、
`CT_ERR_PARAM`（話速が範囲外）、`CT_ERR_MEMORY`（`ct2_synthe_wav` のみ）、`CT_ERR_VOICE` です。

## 仕組み

合成の仕組みは V1 と同じです（[ColorfulTalk-V1 の design.md](https://github.com/ukagaka-irono/ColorfulTalk-V1/blob/main/docs/design.md)）。
V2 では声の構造体を ROM に固定せず、`ct2_start` で声データから作業領域に写して使います。
