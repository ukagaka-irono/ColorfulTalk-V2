# ColorfulTalk V2

声の種類を決める 16KB 程度のデータを差し替えることで、実行中に 14 種類の声を切り替えられる、
無料（MIT ライセンス）の日本語音声合成エンジンです。[ColorfulTalk シリーズ](https://github.com/ukagaka-irono/ColorfulTalk) の一つです。

> **状態: 設計中。** ソースコードはまだありません。仕様は [docs/spec.md](docs/spec.md) で検討しています。
> すぐに使いたい場合は後継の [ColorfulTalk V3](https://github.com/ukagaka-irono/ColorfulTalk-V3) を使ってください。

## 特長（予定）

- 16bit 以上の CPU で、整数演算だけで動く
- 声データ（`.ctv`、16KB 以下）をポインタで渡して声を切り替える。ROM・フラッシュ・RAM のどこに置いてもよい
- 14 種類の声データを同梱。声データを作るツールも用意する
- 入力はカナ記号列（V1・V3 と共通）。書き方は [docs/koe-kana.md](docs/koe-kana.md)
- 出力は 16bit PCM、8kHz、モノラル
- RAM 5KB 以下、ROM 51KB 以下（声データを除く）

## 配布の予定

| 対象 | 配布物 |
|---|---|
| Windows / macOS / Linux | ビルド済みのライブラリ、ヘッダ、声データ 14 種 |
| OS のない組み込み機器 | ソースコード一式と声データ |

MIT ライセンスなので、声や関数の形を変えたいときは自由に改変できます。

## 文書

| 文書 | 内容 |
|---|---|
| [docs/spec.md](docs/spec.md) | 仕様書（案） |
| [docs/voice-data.md](docs/voice-data.md) | 声データ形式（案） |
| [docs/koe-kana.md](docs/koe-kana.md) | カナ記号列 |
| [docs/errors.md](docs/errors.md) | エラーコード |

## ライセンス

MIT License。詳細は [LICENSE](LICENSE)。
