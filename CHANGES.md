# 変更履歴

## v3.0.2 (未リリース)

親指Shiftを左右とも左Shiftに統一した (issue #37)。以前は右を `RSFT_T(KC_N)` に
していたが、`S(KC_x)` の weak Shift は必ず左Shiftなので、右Shiftを押している間だけ
ホストから見て「右Shiftを離す → 左Shiftを押す」と修飾が入れ替わり、その1打鍵だけ
Shiftが効かない罠があった (issue #18、issue #17)。左に揃えると実Shiftと weak Shift が
同じビットになるので、この入れ替えが原理的に起こらなくなる。

そのままだとQMKの修飾ビットが左右で共有されて参照カウントも無いため、片方を
押したままもう片方を離す「持ち替え」でShiftが落ちてしまう。押しっぱなしのほうが
残っているうちは離すイベントを `process_thumb_shift()` で消費し、持ち替えを
途切れさせないようにした。**打鍵時の見た目の挙動は従来どおりで変わらない。**

親指Shiftの左右統一 (issue #37) を受けて、`process_shift_pair()` と
`process_kana_qmark()` を単純化した (issue #39)。どちらも issue #18 対策として
「実Shiftを外す→shifted側のweak Shiftを付ける→外す」という入れ替えを避けていたが、
実Shiftとweak Shiftが常に同じ左Shiftになったことで入れ替えそのものが起こらなく
なったので、素直に unregister → 送出 → register の形へ戻した。外付けキーボードの
右Shift併用は考えない。

---

## v3.0.1

かなレイヤーの「も」(`LT(2,KC_M)`) をShiftを押しながらタップすると、半角「?」を
入力できるようにした (issue #17)。Symレイヤーの数字・記号と同じ要領で、
`KC_LNG2` (英数) → Shift+`KC_SLSH` → `KC_LNG1` (かな) の順に送出する。このとき
押しっぱなしの実Shift (親指Shiftを含む) は、モード切り替えの間だけ外して
「?」でそのまま戻して使う。Shiftが乗ったままだとIMEが英数/かなキーとして
受け取らずかなのまま「・」が出てしまい、逆に左Shiftを自前で付け直すと
修飾が入れ替わって見えて半角「/」が出てしまうため (issue #18 と同じ罠)。
ホールドは従来どおりSymレイヤーへの遷移のまま変わらない。

起動時のベースレイヤーを かな → 英数 に変更した (issue #22)。英数/かなの
切り替え (`MY_LCTL`) はEEPROMに保存しないので、`keyboard_post_init_kb()` で
毎回の電源投入時に `default_layer_state` を直接書き換えて英数へ強制する。

---

## v3.0.0

QMKを **0.14.25 → 0.33.11** へ更新。あわせて、当時のQMKに無かった機能を自前実装していた
部分を捨てて、[geonix41/minipeg48](https://github.com/cognitom/qmk_firmware_geonix41/tree/geonix41-customized-layout/keyboards/geonix41/minipeg48)
で書き直したQMK標準ベースの実装へ移植した。

そのままではビルドできなくなっていたのが発端。QMK 0.14 以降で
`config_common.h` / `RESET` / `RGB_TOG` / `DRIVER_LED_TOTAL` / `RGB_DI_PIN` /
`RGBLED_NUM` / `NO_ACTION_MACRO` などが軒並み削除されている。

**LEDの光らせ方 (キー種別ごとの配色) と、明るさ 強/弱 のトグルは従来どおり。**

---

### 破壊的変更: キー配列

#### かな配列が変わった

以前は `KA_*` という独自キーコードを定義し、firmware 内で JISかな相当の ASCII
(例: 「あ」→ `KC_3`) に変換していた。そのうえで、英語配列では修飾キーが並ぶ最下段も
かなに使う独自の4段配置にしていた。

現在は **firmware 側で一切変換せず、OSのIMEの「かな入力」に任せる**。レイヤー0を
素のQWERTY+数字段にしてあるので、標準のJISかな配置がそのまま出る。

#### レイヤー構成: 7 → 4

| 旧 | 新 |
|--|--|
| `_ALPHA` / `_ALPHA_SHIFTED` / `_NUMPAD` / `_KANA` / `_KANA_SHIFTED` / `_SYM` / `_FN` | `LAYER_KANA`(0) / `LAYER_ALPHA`(1) / `LAYER_SYM`(2) / `LAYER_FN`(3) |

- `_ALPHA_SHIFTED` / `_KANA_SHIFTED` は、Shift時の出し分けをテーブル引き
  (`my_shift_pairs[]`) に置き換えたため不要になった
- `_NUMPAD` は廃止 (移植元に無い)
- レイヤー0とレイヤー1がベースレイヤーで、`default_layer_set()` で切り替える。
  レイヤー1で透過のキーはQMKの仕様どおりレイヤー0へ落ちる

#### レイヤー0 — かな

|  |  |  |  |  |  |  |  |  |  |  |  |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Esc | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | Enter |
| Tab | Q | W | E | R | T | Y | U | I | O | P | [ |
| BS | A | S | D | F | G | H | J | K | L | ; | ' |
| MY_LCTL | Z | X | C | V | B | N | M | , | . | / | ` |

最下段のホールド動作:

| キー | タップ | ホールド |
|--|--|--|
| 左端 (`MY_LCTL`) | 英数 (2回タップでかな) | Ctrl + 英数レイヤー |
| Z | Z | GUI |
| X | X | Alt |
| C / , | C / , | Fn (レイヤー3) |
| V / M | V / M | Sym (レイヤー2) |
| B / N | B / N | Shift |

#### レイヤー1 — 英数

|  |  |  |  |  |  |  |  |  |  |  |  |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| (Esc) | Q | W | E | R | T | Y | U | I | O | P | (Enter) |
| (Tab) | A | S | D | F | G | H | J | K | L | ; | ' |
| (BS) | Z | X | C | V | B | N | M | , | . | ↑ | → |
| (MY_LCTL) | GUI | Alt | Fn | Sym \ | Space | Space | Sym / | Fn | App | ← | ↓ |

括弧付きは透過でレイヤー0から落ちてくるもの。Space はホールドで Shift (SandS)。
風車状のカーソル配置は従来どおり。

#### レイヤー2 — Sym

|  |  |  |  |  |  |  |  |  |  |  |  |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | |
| | ! | @ | # | $ | % | ^ | & | * | ( | ) | ` |
| | = | + | - | _ | [ | ] | ~ | { | } | ↑ | → |
| | | | | | \| | ? | | | | ← | ↓ |

#### レイヤー3 — Fn

|  |  |  |  |  |  |  |  |  |  |  |  |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| MY_WIN | | | MY_ANDR | | | | | | | QK_BOOT | MY_DARK |
| F1 | F2 | F3 | F4 | F5 | F6 | F7 | F8 | F9 | F10 | F11 | F12 |
| Del | PrtSc | | | | 輝度- | 輝度+ | ミュート | 音量- | 音量+ | ↑ | → |
| | | | | | | | | | | ← | ↓ |

#### Shiftで別の記号を出すキー

IMEをかな入力にしていると通常の位置では打てない記号を、Shift側に逃がしている。

| キー | W | R | U | [ | K | L | ; | ' | A | O | P |
|--|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Shift時 | + | \ | - | ] | < | > | ? | _ | Z | 「 | 」 |

親指Shift (B/N) を押しながらそのキー自身をタップすると半角スペース。ただしShift開始後に
他のキーを押していた場合は、誤入力ガードとして何も出さない。

#### `MY_LCTL` — 英数 / かな / Ctrl

| 操作 | 動作 |
|--|--|
| 1回タップ | `KC_LNG2` (英数) + ベースレイヤーを1へ |
| 2回タップ | `KC_LNG1` (かな) + ベースレイヤーを0へ |
| ホールド | Ctrl + 英数レイヤー |

ホールド中は英数レイヤーになるので、かな入力中でも <kbd>Ctrl</kbd>+<kbd>C</kbd> などが
そのまま打てる。旧版の「かな入力時にCtrl+矢印をキャンセルする」ワークアラウンドは不要に
なったので削除した。

かなのままSymレイヤーの数字・記号を打った場合は、`KC_LNG2` → キー → `KC_LNG1` の順に
送出して全角になるのを防ぐ (IMEの切り替えが非同期なので間に30msのウェイトを挟んでいる。
`LT2_IME_WAIT_MS` で調整可)。

#### 廃止した機能

- **ローマ字かなエミュレーション** (`JA_ROKA`) — 「た」で "ta" を送出していたもの
- **入力モードの切り替え** (`JA_ROME` / `JA_ROKA` / `JA_KANA`)
- **IME種別5種** (`IME_WIN` / `IME_AND` / `IME_CRM` / `IME_MAC` / `IME_IOS`)
  → 「」の出し分け用に `MY_WIN` / `MY_ANDR` の2種のみ残した
- **LEDインジケーター** — IME種別・言語モードのキーを白く光らせていたもの
- **`_NUMPAD` レイヤー**
- **独自の tap-hold** (Mod Seq / Quick Tap)

#### キーコードの変更

| 旧 | 新 |
|--|--|
| `RGB_TOG` (LED明暗トグル) | `MY_DARK` — 物理位置は Fn+Enter のまま |
| `RESET` | `QK_BOOT` — Fnレイヤー最上段の右から2番目 |
| `KA_*` (67個) | 廃止 |
| `IME_WIN` 〜 `IME_IOS` | `MY_WIN` / `MY_ANDR` |
| `JA_ROME` / `JA_ROKA` / `JA_KANA` | 廃止 |

カスタムキーコードの起点も `SAFE_RANGE` から `QK_KB_0` へ変更した。

---

### 独自実装 → QMK標準への置き換え

`firmware/windmill.c` は 939行 → 約400行になった。

| 旧 (独自実装) | 新 |
|--|--|
| Mod Seq (`windmill_modtap` / `windmill_layertap` / `windmill_modlayertap`) | `MT()` / `LT()` + `hold_on_other_key_press` |
| Quick Tap (`SECOND_TAPPING_TERM` / `THIRD_TAPPING_TERM`) | 廃止。`tapping.term` (200ms) のみ |
| `windmill_tap_code()` (独自キーコード対応の拡張版 `tap_code`) | 廃止 |
| `KA_*` + `translate_kana_to_ascii()` | 廃止。OSのIMEに任せる |
| `tap_code_roka()` (ローマ字かなエミュレーション) | 廃止 |
| ALT/GUIのweakmod (単打時に修飾を送らない) | `LGUI_T()` / `LALT_T()` |
| かな入力時のCtrl+矢印の回避 (`is_ctrl_canceled`) | 廃止。Ctrlホールド中は英数レイヤーなので不要 |
| `is_kana` のEEPROM保存 | 廃止。`default_layer_set()` で切り替え (EEPROMを書かない) |
| SandS の自前実装 | `LSFT_T(KC_SPC)` |

`TAPPING_TERM = 0` + 独自処理をやめ、`hold_on_other_key_press` を使うようにした。
`permissive_hold` では「シフトdown→キーdown→シフトup→キーup」の順でシフトが効かない
問題があったが、`hold_on_other_key_press` は「ホールド中に他のキーが押された時点で
ホールド確定」なので、離す順序に依存しない。

#### 残っている独自実装 (`firmware/windmill.c`)

- `MY_LCTL` の tap / double-tap / hold の状態機械 (`td_phase`)。
  QMKのTap Danceではなく手書きなのは、別キー割り込みで tapping term を待たずに
  ホールド確定させたい (`pre_process_record_kb()`) のと、確定時に `default_layer_set()`
  も呼びたいため
- `process_shift_pair()` + `my_shift_pairs[]` — QMKの Key Override 相当。
  **`MY_W` 〜 `MY_A` は `keycode - MY_W` でテーブルを引くので、`windmill.h` の enum の
  並び順を変えないこと**
- `process_thumb_shift()` — 親指Shift中のタップで半角スペース
- Symレイヤーの数字・記号のIMEラップ

---

### 維持したもの: LEDの配色

キーコードの種類ごとに色を割り当てる独自実装は、そのまま残している。

- `keymap.c` の `windmill_process_keycolor_user()` がキーコードを配色カテゴリに分類
- `keymap.c` の `colorset[][6]` が `{明るい時のRGB, 暗い時のRGB}` を持つ
  (technikは暗め、ymd40は明るめで別々の値)
- 各キーボードの `.c` の `lighting_map[]` が「行×列」の並びをLEDの番号へ対応させる
- 起動時に全レイヤー分を `cached_keycolormap[]` に焼き、レイヤーが変わるたびに上の
  レイヤーから透過でない色を拾って `cached_keycolors[]` を作り直す
- 10分間操作がないと消灯。次の打鍵で点灯し、その打鍵自体は入力されない
- 明暗トグルはEEPROMに保存され、USBを挿し直しても保持される

配色まわりで直したところ:

- **`default_layer_state_set_kb()` を追加** — `MY_LCTL` がベースレイヤーを切り替えるので、
  これがないと かな⇔英数 でLEDが追従しない
- **`windmill_base_keycode()` を追加** — 新キーマップは `LT(2,KC_V)` / `LSFT_T(KC_B)` /
  `LGUI_T(KC_Z)` のような合成キーコードを多用するため、分類前にタップ側へ展開する。
  `S(KC_1)` などのシフト付きは展開せず、記号として分類する
- **`windmill_process_keycolor_user()` にレイヤー番号を渡すようにした** —
  かなレイヤー (レイヤー0) はOSのIMEが変換するので、キーコードと実際に入る文字が
  食い違う (`KC_1` は「ぬ」、`KC_SLSH` は「め」)。キーコードで分類すると数字段や記号キーが
  数字・記号の色になってしまうため、レイヤー0だけは `Esc` / `Enter` / `Tab` / `BS` /
  英数かな (`MY_LCTL`) を `CL_SPECIAL`、残りは全て `CL_BASE` に固定する。
  v2.0.2 では `_KANA` レイヤーが `KA_*` 専用キーコードだったのでこの問題は起きなかった
- **色の解決をQMKのキーコード解決と揃えた** — `layer_state | default_layer_state` を
  上から走査し、どれも透過ならレイヤー0の色 (`layer_switch_get_layer()` と同じ挙動)
- `rgb_matrix_mode()` / `rgb_matrix_sethsv()` は毎起動でEEPROMを書くので `_noeeprom` 版へ
- 消灯タイムアウト時の `rgblight_sethsv()` も `_noeeprom` 版へ (10分ごとにEEPROMを
  書いていた)
- ymd40 (RGBLight) は消灯からの復帰時に塗り直していなかったので、`apply_keycolors()` を
  呼ぶようにした
- `LAYER_SIZE` 8 → 4
- `rgb_matrix_indicators_kb()` のシグネチャが `void` → `bool` に変わったのに追従

---

### 対応キーボードに minipeg48 を追加

移植元だった [minipeg48](https://github.com/ChrisChrisLoLo/minipeg48)
(Pro Micro互換の4x12オーソリニア) 自体を3機種目として取り込んだ。
定義とキーマップは
[cognitom/qmk_firmware_geonix41](https://github.com/cognitom/qmk_firmware_geonix41/tree/geonix41-customized-layout/keyboards/geonix41/minipeg48)
から持ってきたもので、本家QMKには収録されていない。

    technik / ymd40 / minipeg48

キー配列は3機種とも同一。minipeg48 だけの差分は以下。

#### LED非搭載

`windmill.h` に `WINDMILL_LED_ENABLE` を導入し、LEDが載っている機種
(`RGB_MATRIX_ENABLE` または `RGBLIGHT_ENABLE`) でのみ配色処理をコンパイルする
ようにした。従来は `apply_keycolors()` の内側だけが `#ifdef RGB_MATRIX_ENABLE` で
分岐していて、`extern const uint8_t lighting_map[]` や `cache_keycolors()` の
呼び出しは無条件だったため、LED非搭載機ではリンクが通らなかった。

これに伴い minipeg48 では以下が丸ごと外れる。

- 配色エンジン一式 (`cache_keycolors` / `refresh_keycolors` / `apply_keycolors` /
  `toggle_darkmode` / 消灯タイムアウト)
- `layer_state_set_kb()` / `default_layer_state_set_kb()` (塗り直し専用のため)
- `keymap.c` 側の `colorset[][6]` と `windmill_process_keycolor_user()`
- `lighting_map[]` (= `minipeg48.c` そのものが不要)
- `cached_keycolormap[4][48]` + `cached_keycolors[48]` の 240 バイトのRAM

`MY_DARK` (明るさ 強/弱) はレイヤー3に置いていない。ただし
`enum windmill_keycodes` には3機種共通で残してある (キーコードの値をずらさないため)。
`windmill_config_t` の `led_darkmode` ビットも、EEPROMのレイアウトを揃えるため残した。

#### レイヤー3の上段

| | technik / ymd40 | minipeg48 |
|--|--|--|
| 11列目 | `QK_BOOT` | `QK_BOOT` |
| 12列目 | `MY_DARK` | (なし) |

移植元にはそもそも `QK_BOOT` が無かったが、他の2機種に揃えて同じ位置に置いた。

#### `keyboard.json`

移植元からの変更点:

- **`tapping.term` / `tapping.hold_on_other_key_press` を明記した**。移植元は Vial の
  QMK Settings のデフォルトに頼っていて `tapping` セクションが無かった。これがないと
  親指Shiftや `LT()` の挙動が他の2機種とずれる
- `mousekey` / `nkro` を無効化して technik / ymd40 と揃えた
- `community_layouts: ["ortho_4x12"]` を追加
- USB (`0xBEAF` / `0x0004`)、マトリクスピン、`LAYOUT_ortho_4x12` は移植元のまま

#### Vialキーマップは持ち込んでいない

移植元では `keymaps/vial` (`VIAL_ENABLE`) が主なビルド対象だったが、これは
Vial-QMK フォークでないとビルドできず、このリポジトリの Docker (素の QMK 0.33.11) では
通らない。`default` のみを移植した。Vial版が必要になったら、Vial-QMK 側に
`keyboards/windmill` を置いて `keymaps/vial` を足す形になる。

---

### リポジトリ構成

#### `keyboard.json` への統合

`info.json` + `config.h` + `rules.mk` + `<keyboard>.h` を `keyboard.json` 1ファイルに
まとめた。`technik.h` / `ymd40.h` の `LAYOUT_ortho_4x12` マクロは QMK が自動生成するので
不要になった (`QMK_KEYBOARD_H` が生成ヘッダを指す)。

`config.h` も作っていない。`tapping.term` と `hold_on_other_key_press` が
データ駆動になったため、残す設定が無くなったから。

```
firmware/
  windmill.h                        共有: カスタムキーコード + レイヤー番号
  windmill.c                        共有: キー処理 + LEDエンジン
  technik/
    keyboard.json                   ← info.json + config.h + rules.mk + technik.h
    technik.c                       lighting_map[] のみ (g_led_config は json へ移動)
    readme.md                       qmk lint --strict が要求する
    keymaps/default/keymap.c        keymaps[] + colorset[] + 配色の分類
  ymd40/                            (同構成)
  minipeg48/
    keyboard.json
    readme.md
    keymaps/default/keymap.c        keymaps[] のみ (LED非搭載なので配色なし)
```

- **ymd40 の独自定義を残した理由**: 本家QMKの `ymdk/ymd40/v2` は RGB が 8灯の想定だが、
  実機は 51灯。本家のキーマップとして書く方式は採れない
- **technik** は本家 `boardsource/technik_o` とマトリクスピン・LED配置が完全一致するので、
  `keyboard.json` はそちらをベースにした
- **ymd40 の 2u バリアント4レイアウト** (`LAYOUT_ortho_4x12_2x2u` など) を削除した。
  `info.json` に宣言だけあってマクロの実体が `ymd40.h` に無く、使えない状態だったため
- `build.lto: true` を有効化。RGBのアニメーションは自前描画しか使わないので全て無効化

#### 削除したファイル

```
firmware/technik/{info.json, config.h, rules.mk, technik.h}
firmware/ymd40/{info.json, config.h, rules.mk, ymd40.h}
```

**`NO_ACTION_TAPPING`** が `config.h` にあった。独自 Mod Seq 用の設定だが、QMK標準の
tap-hold へ移行した以上これを残すと `LT`/`MT` が全く効かなくなる。

---

### ビルド / CI

#### `scripts/`

- `Dockerfile`: `ARG QMK_VERSION` を 0.14.25 → **0.33.11**
- `build.sh`: Dockerイメージのタグに QMK のバージョンを含めるようにした。以前は
  `windmill-qmk` 固定だったので、バージョンを上げてもイメージが作り直されなかった。
  現行QMKで未使用の `ALT_GET_KEYBOARDS` を削除
- `entrypoint.sh`: `set -euo pipefail` を追加。以前はビルドが失敗しても `mv` に進んで
  終了コード0で終わっていた。あわせてビルド前に `qmk lint --strict` を走らせるようにした。
  ビルド対象は `technik ymd40 minipeg48` の3機種

#### `.github/workflows/main.yml`

- **リリースへのアップロードが失敗していたのを修正**。`asset_path` が
  `./output/windmill-technik.hex` (ハイフン) だったが、実際の出力は
  `windmill_technik.hex` (アンダースコア) だった
- **ymd40 の hex がリリースに含まれていなかった**ので、両方添付するようにした
- archived な `actions/create-release@v1` + `actions/upload-release-asset@v1` を
  `softprops/action-gh-release@v2` の1ステップへ。`actions/checkout@v2` → `@v4`
- タグのpush時だけでなく、**通常のpush / PR でもビルドを走らせるジョブ**を追加した

#### リリース資産名の変更

| 旧 | 新 |
|--|--|
| `windmill.hex` (technikのみ) | `windmill_technik.hex` / `windmill_ymd40.hex` / `windmill_minipeg48.hex` |

---

### ビルド検証

QMK 0.33.11 + avr-gcc 7.3 でビルド。警告なし、`qmk lint --strict` 通過。

| | Flash | RAM |
|--|--|--|
| technik | 16622 / 28672 (57%) | 1103 / 2560 (43%) |
| ymd40 | 17422 / 28672 (60%) | 844 / 2560 (33%) |
| minipeg48 | 13090 / 28672 (45%) | 331 / 2560 (12%) |

配色の分類については、`keymap.c` から分類関数とキーマップを機械的に抜き出して
ホスト側で全192キー分を評価し、意図したカテゴリになることを確認した。

`WINDMILL_LED_ENABLE` の導入では、technik / ymd40 のFlash・RAMともに導入前と
1バイトも変わっていないこと、上記の配色チェックの出力が完全一致することを確認している。

実機での確認状況:

| | |
|--|--|
| かな⇔英数 の切り替えと、それに追従するLEDの色替え | 確認済み (かなレイヤーの配色は後述の修正あり) |
| `MY_DARK` の明暗トグルと、USB挿し直し後の保持 | 確認済み |
| 親指Shift (tap=B/N, hold=Shift, Shift中のtap=半角スペース) | 確認済み |
| かなモードでSymレイヤーの数字・記号が全角にならないか | 確認済み |
| `MY_WIN` / `MY_ANDR` による 「」 の出し分け | 未確認 |
| **minipeg48** | **未確認** |

---

### ドキュメントについて

`README.md` と `docs/` 配下は、上記の変更に合わせて作者が更新済み
(旧い内容は `docs/archived/` へ移動)。

**未反映なのは minipeg48 の追加分のみ。** `README.md` の「対応キーボード」が
Technik と YMD40 の2つのままなので、必要であれば
[minipeg48](https://github.com/ChrisChrisLoLo/minipeg48) を足すこと
(市販品ではなく、PCBを自作するタイプ)。

Androidで使う場合の注意 (移植元の readme より):

- Android側の「物理キーボードのレイアウト」を **「英語（アメリカ）」** にすること。
  このキーボードはUS配列 (ANSI) としてキーコードを送るため、日本語IMEを使う場合でも
  「日本語 109A 配列」ではなく「英語（アメリカ）」が正しい。109A配列のままだと
  Android側がJIS配列として解釈するので `@` `[` `]` `:` などが入らない
- あわせてFnレイヤーの `MY_ANDR` を押しておくこと

---

## v2.0.2 以前

コミット履歴を参照。
