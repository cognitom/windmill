# CLAUDE.md

Windmill は40%キーボード向けのQMKキーマップ。対応機種は technik / ymd40 /
minipeg48 / geonix41 の4つで、**キー処理の本体は `firmware/windmill.c` に集約**し、
機種ごとのディレクトリはキーマップと配線だけを持つ。

## 構成

| パス | 中身 |
|--|--|
| `firmware/windmill.c` `firmware/windmill.h` | 全機種共通のキー処理。レイヤー、Shift出し分け、親指Shift、LED |
| `firmware/<機種>/` | `keyboard.json`, `rules.mk`, `keymaps/default/keymap.c` |
| `scripts/` | Docker越しのlint (`lint.sh`) とテスト (`test.sh`)、ビルド (`build.sh`)、リリース (`release.sh`) |
| `patches/` | QMK本体へ当てるパッチ。コンテナ内でのみ使う |
| `tests/` | QMKのテスト基盤に載せたユニットテスト。詳細は `tests/readme.md` |
| `docs/` | 導入方法、ビルド手順 |
| `CHANGES.md` | 変更履歴。`release.sh` が生成する |

`output/` と `vendor/` は生成物なのでコミットしない (`.gitignore` 済み)。
`vendor/` は geonix41 のベンダー配布物で、`scripts/fetch-vendor-blob.py` が取ってくる。

## lintとテストとビルド

どれも Docker が要る。QMKのバージョンは `scripts/Dockerfile` の `ARG QMK_VERSION` が正。

```bash
bash scripts/lint.sh    # keyboard.json の静的チェック。イメージが在れば数秒
bash scripts/test.sh    # ユニットテスト。イメージのビルドから走ると数分かかる
bash scripts/build.sh   # 4機種ぶんのファームウェアを output/ に出す。数分かかる
```

**コードを触ったら `scripts/lint.sh` と `scripts/test.sh` を通す。`scripts/build.sh`
は時間がかかるので普段は走らせなくてよい。** 4機種ぶんのコンパイルはタグを打った
ときの Release ワークフローが持つ。PRを出せば Test ワークフロー
(`.github/workflows/test.yml`) が lint とユニットテストを走らせる。

ただし手元の2つが見る範囲は限られる。ここに引っかからない変更もある。

- lint (`qmk lint --strict`) が読むのは `keyboard.json` とレイアウト定義だけ。
  Cのコードは1行も見ない
- テストは `firmware/windmill.c` をホスト向けにコンパイルして動かす。
  機種ごとの `keymaps/default/keymap.c` と `rules.mk` は通らない
  (テスト側は `tests/test_keymap.hpp` を使う)

だから **`keymap.c` や `rules.mk` を触ったときは `scripts/build.sh` も通しておく**。
`firmware/windmill.c` だけの変更なら lint とテストで足りる。

## リリース

`main` で `scripts/release.sh` を実行する。次のバージョン番号を決めて、
`CHANGES.md` を書き足してコミット・プッシュし、タグを打つところまでやる。

```bash
bash scripts/release.sh              # パッチ版を上げる (v3.0.1 → v3.0.2)
bash scripts/release.sh --minor      # マイナー版を上げる (v3.0.2 → v3.1.0)
bash scripts/release.sh v4.0.0       # バージョンを直接指定する
bash scripts/release.sh --dry-run    # CHANGES.md へ書く内容だけ見て終わる
```

書き足す中身は、前回のタグ以降に main へ入ったPRのリンクを並べただけ。
`git log --first-parent` から拾うので、**PRのタイトルがそのまま履歴の1行になる**。
タグを push すると Release ワークフロー (`.github/workflows/release.yml`) が
4機種ぶんビルドして、GitHubのリリースへファームウェアを貼る。

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
- **`CHANGES.md` は手で書かない。** リリースのたびに `release.sh` が生成する。
  かわりに、PRのタイトルだけ読んで何が変わったか分かるように付ける
- ユーザー向けの挙動が変わったら `docs/` を更新する。
  キー配列を変えたら `docs/images/layout-*.png` も古くなるので、その旨を伝える
- QMKのバージョンを上げるときは `scripts/Dockerfile` の `ARG QMK_VERSION` を変える。
  ビルドスクリプトとCIのキャッシュキーはそこを見ている

## 用語

- **SandS** — Spaceをホールドすると Shift になる。実装は `process_thumb_shift()`
- **Shift出し分け** — `my_shift_pairs[]` のテーブル引きで、Shift時に別のキーを出す。
  実装は `process_shift_pair()`
- **レイヤー** — `LAYER_KANA`(0) / `LAYER_ALPHA`(1) / `LAYER_SYM`(2) / `LAYER_FN`(3)。
  0と1がベースレイヤーで `default_layer_set()` で切り替える
