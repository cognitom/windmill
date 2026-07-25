# キーマップで使える機能

## 拡張版 tap_code()

Windmill内で定義されたキーコードは`tap_code()`で処理できないので、下記の関数を使う。

```cpp
windmill_tap_code(KA_A);
```

## Mod Seq

QMKの[MT](https://docs.qmk.fm/#/mod_tap)や[LT、LM](https://docs.qmk.fm/#/feature_layers)に近い機能を提供するもの。相違点としては次の通り。

- キーを押した時点で、修飾キーが送出される (または、レイヤーが有効化される) → `TAPPING_TERM = 0`
- 素早く(`SECOND_TAPPING_TERM`以内で)複数のキーがタップされた場合は、修飾キーをキャンセルして通常キーとして扱う → Quick Tap参照
- 指定時間内(`THIRD_TAPPING_TERM`以内)で単独キーがタップされた場合も、修飾キーをキャンセルして通常キーとして扱う
- 通常キーと修飾キーとレイヤーの同時指定ができる (LT、LMを1キーで実現する)
- 複数の修飾キーとレイヤーの同時押しを考慮する ※例. 左右のシフトキーを片方だけ残しつつ、押したり離したりする場合

`keymap.c`で設定するが、`keymaps[]`配列では指定せず、`process_record_user()`で次のように書く。

```cpp
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  switch (keycode) {
    case KC_SPC:
      return windmill_modlayertap(keycode, record, MOD_MASK_SHIFT, _ALPHA_SHIFTED);
  }
  return true;
}
```

使用可能なのは、次の3つの関数。

- `windmill_modlayertap(keycode, record, mod_mask, layer_to_activate);`
- `windmill_layertap(keycode, record, layer_to_activate);`
- `windmill_modtap(keycode, record, mod_mask);`

## Quick Tap

Mod Seqの一部として動作し、素早く(`SECOND_TAPPING_TERM`以内で)複数のキーがタップされた場合に、通常キーの連続として扱うための機構。キー押下時に即時送出せず、バッファしている。`keymap.c`から直接使用することはないが、内部的に動作している。
