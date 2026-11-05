# ColorfulTalk V2

声の種類を決める小さなデータ（`.ctv`）を差し替えることで、実行中に 14 種類の声を切り替えられる、
無料（MIT ライセンス）の日本語音声合成エンジンです。[ColorfulTalk シリーズ](https://github.com/ukagaka-irono/ColorfulTalk) の一つです。
より自然な声やパラメータでの細かな調整が必要なら、後継の [ColorfulTalk V3](https://github.com/ukagaka-irono/ColorfulTalk-V3) を使ってください。

- 録音した声を使わない **フォルマント合成**。声データのライセンスを気にせず組み込んで配布できる
- **整数演算だけ** で動く。`int` が 16bit の CPU でも PC と同じ出力になる（AVR のシミュレータでビット単位の一致を確認）
- ROM 約 24KB、RAM 約 4.7KB（作業領域 4.2KB＋スタック）、動的メモリ不使用
- 声データは 1 つ 212 バイト（上限 16KB）。ROM・フラッシュ・RAM のどこに置いてもよく、`ct2_start` に渡すだけで切り替わる
- 14 種類の声データを同梱（女声 5・男声 4・子ども 2・年配・ささやき・ロボット）。`mkvoice` で自分の声も作れる
- 入力はカナ記号列（V1・V3 と共通）。書き方は [docs/koe-kana.md](docs/koe-kana.md)
- 出力は 16bit PCM、8kHz、モノラル

## 使い方

```c
#include "colorful_talk_v2.h"

static uint8_t work[4352];      /* ct2_work_size() 以上、4 バイト境界 */
int16_t buf[64];
int n;

/* voice: f1.ctv の中身（ROM の配列でも、ファイルから読んだ RAM でもよい） */
if (ct2_start(work, "こんにちわ、せか'い。", voice, 100) == CT_OK) {
    while ((n = ct2_read(work, buf, 64)) > 0) {
        /* buf[0..n) を DAC へ（8kHz） */
    }
}
```

## 文書

| 文書 | 内容 |
|---|---|
| [docs/spec.md](docs/spec.md) | 仕様書（API、同梱の声、寸法） |
| [docs/voice-data.md](docs/voice-data.md) | 声データ形式（`.ctv`） |
| [docs/koe-kana.md](docs/koe-kana.md) | カナ記号列の書き方 |
| [docs/errors.md](docs/errors.md) | エラーコード |

## ビルド

CMake 3.20 以上と C コンパイラが必要です。

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Release
```

```bash
ctest --test-dir build -C Release
```

| 成果物 | 内容 |
|---|---|
| `colorful_talk_v2_static` | 静的ライブラリ（`ct_fx`・`ct_common` と一緒にリンク） |
| `colorful_talk_v2` | DLL / 共有ライブラリ |
| `ctalk` | 試聴・WAV 出力のツール。`-v f1` のように同梱の声の名前か、`.ctv` ファイルを指定 |
| `mkvoice` | 声データを作るツール |

マイコンへ組み込むときは、次をプロジェクトに加えてください。

- ソース: `src/ct2_api.c`、`src/ct2_voice.c`、`fx/src/*.c`、`common/src/ct_koe.c`
- インクルードパス: `include/`、`src/`、`fx/include/`、`fx/src/`、`common/include/`
- 定義: `CT_NO_WAV`、`CTFX_MAX_MORA=160`、`CTFX_MAX_PHRASE=40`
- 声データ: 使う `.ctv` を C の配列にして加える（`src/ct2_builtin_voices.c` に 14 種の配列があります）

## 声を作る

```bash
build/Release/mkvoice.exe -o myvoice.ctv --name myvoice --f0 240 --formant 1.2 --rd 1.1 --range 1.3
```

```bash
build/Release/ctalk.exe -v myvoice.ctv --play "じぶんで/つくった/こえです。"
```

| オプション | 意味 | 範囲 |
|---|---|---|
| `--f0` | 声の高さの基準 [Hz] | 60〜500 |
| `--formant` | 声道の長さの倍率。大きいほど小柄な声 | 0.7〜1.5 |
| `--rd` | 声の張り。小さいほど張った声、大きいほど息っぽい声 | 0.3〜2.7 |
| `--range` | 抑揚の幅（1 = 標準） | 0〜3 |
| `--breath` | 気息音の量 | 0〜3 |
| `--jitter` | 声のゆらぎ | 0〜3 |
| `--whisper` | ささやき声 | |
| `--level` | 音量の補正 | 0〜4 |

同梱の 14 種は `voices/voices.csv` から作っています（`cmake --build build --target voices`）。

## 16bit CPU での確認

`tests/avr/check.sh` で、V2 を ATmega1284P（`int` は 16bit）向けにビルドし、AVR シミュレータ（avr8js）で動かして
PC 版と出力を比べます。avr-gcc と Node.js（avr8js）が必要です。

```bash
VOICE=f1 AVR_GCC=/path/to/avr-gcc/bin AVR8JS=/path/to/avr8js-folder tests/avr/check.sh "こんにちわ、せか'い。"
```

`tools/make_samples.ps1` で試聴用の WAV（14 種の声）を `samples/v2/` に作れます。

## ライセンス

MIT License。詳細は [LICENSE](LICENSE)。
