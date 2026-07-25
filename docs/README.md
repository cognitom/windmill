# Windmillのメモ書き

Windmillの説明については、[こちら](../README.md)へ。実装の現状は[実装メモ](keymap.md)へ。

以下は2021〜2022年に書いた当時のメモです。QMK 0.14 の頃の話で、**現在は解決済み**の
ものが多いので、各節に現状を追記してあります。

## SandS

タップでSpace、ホールドでShiftにする。Space and Shift あるいは SandS。日本特有の需要なのか、あまり英語の情報を見かけないが、QMKの機能だけでも実装可能。

[keymap.c]()にSandS用のキーを定義。

```
#define SNADS LSFT_T(KC_SPC)
```

[config.h]()にpermissive holdの設定を追加して、tapping termを若干長めに取る。

```c
#define TAPPING_TERM 250
#define PERMISSIVE_HOLD
```

こだわる人は、keymap.cの`process_record_*`コールバックで自力で実装するみたいだけど、~~上記でもたぶん十分~~。

全然十分じゃなかった。[ドキュメントにも記載](https://docs.qmk.fm/tap_hold#permissive-hold)されているけど、permissive holdだけでは、シフトdown→わdown→シフトup→わupの順にキーが押された場合、シフトが有効にならない。一般的な日本語処理系では、上記のケースはシフト有効として判断されるし、打鍵が高速になれば上記のケースが増える。気を付けて打てばどうにかなるというものでもなさそうだ。やはり、`process_record_*`コールバックで自力で実装する必要がある。

**現在** ・ `permissive_hold` ではなく
[`hold_on_other_key_press`](https://docs.qmk.fm/tap_hold#hold-on-other-key-press)
を使っている。こちらは「ホールド中に他のキーが押されたら、離す順序を待たずにホールド確定」
なので、上記の順序問題は起きない。独自の Mod Seq / Quick Tap は廃止した。

## RGB Matrix

### アニメーションを止める

BoardsourceのTechnikは、サンプルとして用意されているファームを入れると、レインボーなアニメーションがひたすら流れ続ける。これを止める方法がしばらくわからなかったが、`keymap.c`で、`keyboard_post_init_*`コールバックで指定すれば、指定が永続した。

```c
void keyboard_post_init_user(void) {
  rgb_matrix_mode(RGB_MATRIX_NONE); // アニメーションなし
  rgb_matrix_sethsv(43, 43, 63); // 色指定
}
```

**現在** ・ `rgb_matrix_mode()` / `rgb_matrix_sethsv()` は毎起動でEEPROMを書いてしまうので
`_noeeprom` 版を使っている。またアニメーションは全く使わないので、`keyboard.json` の
`rgb_matrix.animations` を空にしてビルドから外している。

### 暫くすると消灯させる

キーボードを使っているときだけ、光るようにしたい。1分触らなければ消灯するようにした。`keymap.c`の`refresh_rgb_matrix_timeout`と`process_rgb_matrix_timeout`を参照。

参考・[LED Timeout after n minutes of idle?](https://www.reddit.com/r/MechanicalKeyboards/comments/53nmvk/help_tmkqmk_led_timeout_after_n_minutes_of_idle/)

**現在** ・ `windmill.c` の `update_led_timeout()` / `process_led_timeout()`。10分に変更。

### PCのスリープ時に消灯させる

`config.h`に次の設定を追加。

```c
#define RGB_DISABLE_WHEN_USB_SUSPENDED true
```

https://docs.qmk.fm/features/rgb_matrix#additional-configh-options

ちなみに、`suspend_power_down_*`と`suspend_wakeup_init_*`を使うと書いている記事が多いが、動作しない。RGB Matrixについてはドキュメントがまだ不足しているのかも。

https://docs.qmk.fm/custom_quantum_functions#keyboard-idlingwake-code

**現在** ・ この定義は `RGB_MATRIX_SLEEP` に改名され、さらにデータ駆動化された。
`keyboard.json` の `rgb_matrix.sleep` (RGBLightなら `rgblight.sleep`) で指定する。

## ALT,GUIは、修飾キーかつ単打である問題

ALTもGUIも、他のキーと組み合わせで使う場合と、単打で使う場合がありSandS的な使い方では、この点が問題になる。

Windmillでは、かな入力時、修飾キーとしてのみALT/GUIが有効になるようにしている。実装としては、押下時にすぐには`register_mods`せず、以下のようなフラグを用意して`true`にしている。押下したまま次のキーが押されたら、そのキーコードの直前に`register_mods`する。そうすれば、単打であった場合にはALT/GUIを送出せずに済む。

```cpp
static bool alt_reserved = false;
static bool gui_reserved = false;
```

**現在** ・ 素直に `LGUI_T(KC_Z)` / `LALT_T(KC_X)` を使っている。単打でZ/Xが出て、
ホールドでGUI/Altになる。かな配列を firmware 側で組み替えるのをやめて、OSのIMEに
任せるようにしたことで、独自の weakmod を維持する理由がなくなった。
