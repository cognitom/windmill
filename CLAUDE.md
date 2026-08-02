# CLAUDE.md

Windmill は40%キーボード向けのQMKキーマップ。対応機種は technik / ymd40 /
minipeg48 / geonix41 の4つで、**キー処理の本体は `firmware/windmill.c` に集約**し、
機種ごとのディレクトリはキーマップと配線だけを持つ。

## 構成

| パス | 中身 |
|--|--|
| `firmware/windmill.c` `firmware/windmill.h` | 全機種共通のキー処理。レイヤー、Shift出し分け、親指Shift、LED |
| `firmware/<機種>/` | `keyboard.json`, `rules.mk`, `keymaps/default/keymap.c` |
| `scripts/` | Docker越しのビルド (`build.sh`) とテスト (`test.sh`) |
| `patches/` | QMK本体へ当てるパッチ。コンテナ内でのみ使う |
| `tests/` | QMKのテスト基盤に載せたユニットテスト。詳細は `tests/readme.md` |
| `docs/` | 導入方法、ビルド手順 |
| `CHANGES.md` | 変更履歴 |

`output/` と `vendor/` は生成物なのでコミットしない (`.gitignore` 済み)。
`vendor/` は geonix41 のベンダー配布物で、`scripts/fetch-vendor-blob.py` が取ってくる。

## ビルドとテスト

どちらも Docker が要る。QMKのバージョンは `scripts/Dockerfile` の `ARG QMK_VERSION` が正。

```bash
bash scripts/test.sh    # ユニットテスト。イメージのビルドから走ると数分かかる
bash scripts/build.sh   # 4機種ぶんのファームウェアを output/ に出す
```

コードを触ったら最低限 `scripts/test.sh` は通す。コンパイルが通るかどうかは、
PRを出せば Build ワークフロー (`.github/workflows/main.yml`) が4機種ぶん確かめる。

## 守ってほしいこと

- **キー配置を変えたら `tests/test_keymap.hpp` も直す。**
  テスト側のキーマップは `firmware/technik/keymaps/default/keymap.c` と同じ内容を
  手で二重管理している (実機側は `LAYOUT_ortho_4x12` と PROGMEM に依存していて読めない)
- **キー処理の変更にはレポート列のテストを添える。**
  キーコードだけ合っていても修飾キーの出方が壊れることがある (issue #18 がそれ)。
  `EXPECT_REPORT` でホストへ送出されるレポートを1本ずつ固定する
- **共通の挙動は `firmware/windmill.c` に置く。** 機種ディレクトリへ処理を書き足さない
- **コメントとコミットメッセージは日本語。** 既存のコードとログの書き方に合わせる。
  コメントは「何をしているか」ではなく「なぜそうしたか」を書く
- **キー配列そのものの変更は勝手に決めない。** 配置は作者の好みの問題なので、
  issue で確認してから動く
- ユーザー向けの挙動が変わったら `CHANGES.md` と `docs/` を更新する。
  キー配列を変えたら `docs/images/layout-*.png` も古くなるので、その旨を伝える
- QMKのバージョンを上げるときは `scripts/Dockerfile` の `ARG QMK_VERSION` を変える。
  ビルドスクリプトとCIのキャッシュキーはそこを見ている

## 用語

- **SandS** — Spaceをホールドすると Shift になる。実装は `process_thumb_shift()`
- **Shift出し分け** — `my_shift_pairs[]` のテーブル引きで、Shift時に別のキーを出す。
  実装は `process_shift_pair()`
- **レイヤー** — `LAYER_KANA`(0) / `LAYER_ALPHA`(1) / `LAYER_SYM`(2) / `LAYER_FN`(3)。
  0と1がベースレイヤーで `default_layer_set()` で切り替える
