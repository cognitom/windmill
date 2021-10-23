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

こだわる人は、keymap.cの`process_record_*`コールバックで自力で実装するみたいだけど、上記でもたぶん十分。

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
