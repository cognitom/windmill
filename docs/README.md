# Windmillのメモ書き

Windmillの説明については、[こちら](../README.md)へ。

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

全然十分じゃなかった。[ドキュメントにも記載](https://docs.qmk.fm/#/tap_hold?id=permissive-hold)されているけど、permissive holdだけでは、シフトdown→わdown→シフトup→わupの順にキーが押された場合、シフトが有効にならない。一般的な日本語処理系では、上記のケースはシフト有効として判断されるし、打鍵が高速になれば上記のケースが増える。気を付けて打てばどうにかなるというものでもなさそうだ。やはり、`process_record_*`コールバックで自力で実装する必要がある。

## RGB Matrix

### アニメーションを止める

BoardsourceのTechnikは、サンプルとして用意されているファームを入れると、レインボーなアニメーションがひたすら流れ続ける。これを止める方法がしばらくわからなかったが、`keymap.c`で、`keyboard_post_init_*`コールバックで指定すれば、指定が永続した。

```c
void keyboard_post_init_user(void) {
  rgb_matrix_mode(RGB_MATRIX_NONE); // アニメーションなし
  rgb_matrix_sethsv(43, 43, 63); // 色指定
}
```

### 暫くすると消灯させる

キーボードを使っているときだけ、光るようにしたい。1分触らなければ消灯するようにした。`keymap.c`の`refresh_rgb_matrix_timeout`と`process_rgb_matrix_timeout`を参照。

参考・[LED Timeout after n minutes of idle?](https://www.reddit.com/r/MechanicalKeyboards/comments/53nmvk/help_tmkqmk_led_timeout_after_n_minutes_of_idle/)

### PCのスリープ時に消灯させる

`config.h`に次の設定を追加。

```c
#define RGB_DISABLE_WHEN_USB_SUSPENDED true
```

https://docs.qmk.fm/#/feature_rgb_matrix?id=additional-configh-options

ちなみに、`suspend_power_down_*`と`suspend_wakeup_init_*`を使うと書いている記事が多いが、動作しない。RGB Matrixについてはドキュメントがまだ不足しているのかも。

https://docs.qmk.fm/#/custom_quantum_functions?id=keyboard-idlingwake-code

## ALT,GUIは、修飾キーかつ単打である問題

ALTもGUIも、他のキーと組み合わせで使う場合と、単打で使う場合がありSandS的な使い方では、この点が問題になる。

Windmillでは、かな入力時、修飾キーとしてのみALT/GUIが有効になるようにしている。実装としては、押下時にすぐには`register_mods`せず、以下のようなフラグを用意して`true`にしている。押下したまま次のキーが押されたら、そのキーコードの直前に`register_mods`する。そうすれば、単打であった場合にはALT/GUIを送出せずに済む。

```cpp
static bool alt_reserved = false;
static bool gui_reserved = false;
```
