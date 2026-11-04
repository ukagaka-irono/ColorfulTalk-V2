# ColorfulTalk V2 仕様書

> 状態: 案（設計中）。シリーズ全体の仕様書: [ColorfulTalk](https://github.com/ukagaka-irono/ColorfulTalk)（`docs/spec/v2.md` と同じ内容）

| 項目 | 内容 |
|---|---|
| 位置づけ | 声データを差し替えて、実行中に 14 種の声を切り替えられるエンジン。後継は V3 |
| 対象 | 16bit 以上の CPU、Windows / macOS / Linux |
| 方式 | 規則合成（フォルマント合成）。合成コアは V1 と共通 |
| 入力 | カナ記号列（UTF-8）。[koe-kana.md](koe-kana.md) |
| 出力 | 16bit 符号付き PCM、モノラル、8kHz |
| 演算 | 整数のみ（固定小数点） |
| RAM 目標 | 5KB 以下（作業領域＋スタック。声データは含まない） |
| ROM 目標 | 51KB 以下（コード）＋ 使う声データ |
| 声 | 声データ 1 つが 16KB 以下。14 種類を同梱 |
| ライセンス | MIT |

## 声データ

声の種類を決めるデータ（`.ctv` ファイル、16KB 以下）を、合成を始めるときにポインタで渡します。
ROM・フラッシュ・RAM のどこに置いてもかまいません（実行中にエンジンが書き換えることはありません）。
形式は [voice-data.md](voice-data.md) を参照してください。

| ファイル | 声 |
|---|---|
| `f1.ctv` | 女声（標準） |
| `f2.ctv` | 高めの女声 |
| `f3.ctv` | 落ち着いた女声 |
| `f4.ctv` | 明るい女声 |
| `f5.ctv` | かすれた女声 |
| `m1.ctv` | 男声 |
| `m2.ctv` | 低い男声 |
| `m3.ctv` | 若い男声 |
| `m4.ctv` | 張りのある男声 |
| `c1.ctv` | 子ども |
| `c2.ctv` | 幼い子ども |
| `e1.ctv` | 年配の声 |
| `w1.ctv` | ささやき声 |
| `r1.ctv` | ロボット |

## API（案）

```c
#define CT2_WORK_SIZE  /* 作業領域のバイト数 */

int ct2_voice_check(const void* voice, size_t size);   /* 声データの検査（形式・版・チェックサム） */
int ct2_start(void* work, const char* koe_utf8, const void* voice, int speed);
int ct2_read(void* work, int16_t* buf, int n);
int ct2_error_pos(const void* work);

/* PC 向け */
unsigned char* ct2_synthe_wav(const char* koe_utf8, const void* voice, int speed, int* size, int* err);
void           ct2_free(void* buf);
```

声データは `ct2_start` から合成が終わるまで同じ場所に置いておいてください。
不正な声データには `CT_ERR_VOICE` を返します。

## 声データを作る

`tools/mkvoice`（案）で、声の高さ・声道の長さ・Rd などから `.ctv` を作れるようにします。
同梱の 14 種類もこのツールで作ります。
