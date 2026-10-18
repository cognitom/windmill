/* Copyright 2021-2026 Tsutomu Kawamura
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Windmill 共通処理。
 *
 * キー処理は geonix41/minipeg48 からの移植で、QMK標準の mod-tap / layer-tap /
 * default_layer_set を使う。かな入力はOS側のIMEに任せる (かなレイヤーのキーは、
 * JISかな配列でそのかなが載っている位置のキーを送る)。かなレイヤー最下段の
 * タップとホールドだけは自前で判定する。
 *
 * LEDはキーコードの種類ごとに色を割り当てる独自実装で、こちらは従来のまま。
 * LED非搭載機 (minipeg48) では WINDMILL_LED_ENABLE が立たず、配色処理は
 * 丸ごとコンパイルから外れる。 */

#include "windmill.h"
#include "keymap_japanese.h" // JP_*。出力表を、キーの位置ではなく JIS 配列で出る文字で書くため

#define KEY_COUNT (MATRIX_ROWS * MATRIX_COLS)

/* 機種固有の割り込み口 (windmill.h 参照)。既定は何もしない */
__attribute__((weak)) void windmill_board_post_init(void) {}
__attribute__((weak)) void windmill_board_pre_process_record(uint16_t keycode, keyrecord_t *record) {}
__attribute__((weak)) bool windmill_board_process_record(uint16_t keycode, keyrecord_t *record) {
    return true;
}
__attribute__((weak)) void windmill_board_led_begin(void) {}
__attribute__((weak)) uint8_t windmill_board_host(void) {
    return WINDMILL_HOST_USB;
}

/* 接続先のOS。言語切替 (MY_LCTL) のキーと、かなの「」(KN_LKAKKO / KN_RKAKKO) は
 * OSのIME実装で必要なキーが異なるため、MY_WIN / MY_ANDR で切り替える。
 *
 * 無線機では接続先を切り替えるたびにOSを設定し直すのが手間なので、接続先
 * (WINDMILL_HOST_*) ごとに覚える (issue #58)。1つあたり2bitで、今使っているのは
 * Windows と Android の2種だけだが、Mac / iOS を足せるよう4種ぶん確保してある。
 * EEPROMリセット時は全ての接続先が Windows/デスクトップ向け (0) に戻る */
#define OS_WIN  0 // Windows/デスクトップ
#define OS_ANDR 1 // Android
// 2, 3 は予約 (Mac / iOS 用)。読めても Windows と同じに扱う
#define OS_BITS 2
#define OS_MASK ((1u << OS_BITS) - 1)

/* 接続先のキーボード配列 (WINDMILL_LAYOUT_*)。同じ文字でも送るべきキーコードは
 * 相手がどの配列として受け取るかで変わるので、MY_JIS / MY_US で選ぶ (issue #74)。
 * OSと同じく接続先ごとに覚える。
 *
 * JIS を 0 にしてあるのは既定にするため。EEPROMリセット時だけでなく、この設定が
 * 入る前のファームウェアで使っていた機体も、ここのビットは 0 のまま起動してくる。
 * 2bitなのはOSと揃えただけで、今は2種しか無い */
#define LAYOUT_BITS 2
#define LAYOUT_MASK ((1u << LAYOUT_BITS) - 1)

typedef union {
    uint32_t raw;
    struct {
        /* 旧形式 (接続先を区別しない) のAndroid設定。ビット位置を変えずに
         * 残してあるのは、以前のファームウェアで Android にしていた人の設定を
         * 起動時に引き継ぐため (keyboard_post_init_kb 参照)。移行後は常に0 */
        bool     legacy_android : 1;
        bool     led_darkmode : 1; // LEDを暗いほうの配色にする (LED非搭載機では未使用)
        uint16_t host_os : WINDMILL_HOST_SIZE * OS_BITS; // 接続先ごとのOS。WINDMILL_HOST_* 番目の2bit
        /* 接続先ごとの配列。WINDMILL_HOST_* 番目の2bit。
         * 型を uint16_t にすると、host_os の続きに置くか16bit境界まで送るかが
         * AVR とそれ以外で分かれ、EEPROM上の位置が機種で変わってしまう */
        uint32_t host_layout : WINDMILL_HOST_SIZE * LAYOUT_BITS;
    };
} windmill_config_t;
_Static_assert(sizeof(windmill_config_t) == sizeof(uint32_t), "windmill_config_t が eeconfig_kb の32bitに収まっていない");
static windmill_config_t windmill_config;

/* 接続先の番号は機種側が返す。範囲外が来てもEEPROMの隣の接続先を壊さないよう、
 * 有線扱いに丸める */
static uint8_t current_host(void) {
    const uint8_t host = windmill_board_host();
    return host < WINDMILL_HOST_SIZE ? host : WINDMILL_HOST_USB;
}

/* 接続先の切り替えはキーを押した時点のものを引けば足りるので、キャッシュはせず
 * 毎回EEPROMの写し (windmill_config) から読む */
static uint8_t get_host_os(void) {
    return (windmill_config.host_os >> (current_host() * OS_BITS)) & OS_MASK;
}

static bool is_android(void) {
    return get_host_os() == OS_ANDR;
}

uint8_t windmill_host_layout(void) {
    const uint8_t layout = (windmill_config.host_layout >> (current_host() * LAYOUT_BITS)) & LAYOUT_MASK;
    return layout == WINDMILL_LAYOUT_US ? WINDMILL_LAYOUT_US : WINDMILL_LAYOUT_JIS; // 予約の値は既定に倒す
}

/*
 * 起動時のベースレイヤー
 */

/* 英数/かなの切り替え (MY_LCTL) はEEPROMに保存しない (switch_lang 参照)。
 * つまり起動時のベースレイヤーはEEPROMの値を復元するのではなく、電源を入れる
 * たびに固定で決まる。以前は eeconfig_init_kb() (EEPROM初期化直後のみ呼ばれる
 * コールバック) で英数に切り替えていたが、これは通常の電源投入時には呼ばれず
 * 実機ではレイヤー0(かな)のままだった (issue #22)。keyboard_post_init_kb() は
 * 毎回の起動時に呼ばれるので、ここで直接 default_layer_state を書き換えて
 * EEPROMを経由せずに強制する (default_layer_set() は eeconfig も書いてしまう) */
static void reset_default_layer(void) {
    default_layer_state = (layer_state_t)1 << LAYER_ALPHA;
}

/*
 * RGB Matrix / Light
 */

#ifdef WINDMILL_LED_ENABLE

#define RGBMATRIX_TIMEOUT 10 // 分
#define CL_TRANS 0xFF        // 透過キー。下位レイヤーの色を使う

extern const uint8_t lighting_map[KEY_COUNT]; // 各キーボード側で定義

static uint8_t *colorsetPtr;
static uint8_t  cached_keycolormap[LAYER_SIZE][KEY_COUNT];
static uint8_t  cached_keycolors[KEY_COUNT];
static uint16_t idle_timer      = 0;
static uint8_t  halfmin_counter = 0;
static bool     led_initialized = false;
static bool     led_on          = true;
static bool     led_darkmode    = false;

void windmill_init_keycolors(uint8_t *user_colorset) {
    colorsetPtr = user_colorset;
}

uint16_t windmill_base_keycode(uint16_t keycode) {
    if (IS_QK_MOD_TAP(keycode)) {
        /* タップ側が KC_NO のもの (MY_LCTL) は、タップの意味を windmill.c が
         * 自前で持っている。展開すると KC_NO になって keymap.c 側が
         * MY_LCTL として色を引けなくなるので、そのまま返す */
        const uint16_t tap = QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
        return tap == KC_NO ? keycode : tap;
    }
    if (IS_QK_LAYER_TAP(keycode)) return QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
    return keycode;
}

// キーコードから色を引くのは重いので、起動時に一度だけ全レイヤー分を作っておく
static void cache_keycolors(void) {
    for (uint8_t layer = 0; layer < LAYER_SIZE; ++layer) {
        for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
            for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                uint16_t keycode = keymap_key_to_keycode(layer, (keypos_t){.row = row, .col = col});
                cached_keycolormap[layer][row * MATRIX_COLS + col] =
                    (keycode == KC_TRANSPARENT) ? CL_TRANS : windmill_process_keycolor_user(layer, windmill_base_keycode(keycode));
            }
        }
    }
}

// cached_keycolors をLEDへ流し込む
static void apply_keycolors(void) {
    const uint8_t offset = led_darkmode ? 3 : 0;

#ifdef RGB_MATRIX_ENABLE
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        const uint8_t *color = colorsetPtr + cached_keycolors[i] * 6 + offset;
        rgb_matrix_set_color(lighting_map[i], color[0], color[1], color[2]);
    }
#elif defined(RGBLIGHT_ENABLE)
    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        const uint8_t *color = colorsetPtr + cached_keycolors[i] * 6 + offset;
        rgblight_setrgb_at(color[0], color[1], color[2], lighting_map[i]);
    }
#endif
}

/* 有効なレイヤーを上から順に見て、透過でない最初の色を採用する。
 * どのレイヤーでも透過ならレイヤー0の色 (QMKのキーコード解決と同じ挙動)。
 * default_layer_state_set_kb() からは更新後の値を渡す必要があるので、
 * グローバルを読まずに引数で受け取る。 */
static void refresh_keycolors(layer_state_t layers, layer_state_t defaults) {
    const layer_state_t active = layers | defaults;

    for (uint8_t i = 0; i < KEY_COUNT; ++i) {
        uint8_t keycolor = cached_keycolormap[LAYER_KANA][i];
        for (int8_t layer = LAYER_SIZE - 1; layer >= 0; --layer) {
            if (!(active & ((layer_state_t)1 << layer))) continue;
            if (cached_keycolormap[layer][i] == CL_TRANS) continue;
            keycolor = cached_keycolormap[layer][i];
            break;
        }
        if (keycolor != CL_TRANS) cached_keycolors[i] = keycolor;
    }

    // RGB Matrix は rgb_matrix_indicators_kb() で毎フレーム描き直すので何もしない
#ifndef RGB_MATRIX_ENABLE
    if (led_initialized && led_on) apply_keycolors();
#endif
}

static void toggle_darkmode(void) {
    led_darkmode = !led_darkmode;

    windmill_config.led_darkmode = led_darkmode;
    eeconfig_update_kb(windmill_config.raw);

#ifndef RGB_MATRIX_ENABLE
    if (led_initialized && led_on) apply_keycolors();
#endif
}

static void refresh_led_timeout(void) {
    if (!led_on) {
        led_on = true;
        // 消灯時に消していた分を描き直す (RGB Matrix は毎フレーム描くので不要)
#ifndef RGB_MATRIX_ENABLE
        if (led_initialized) apply_keycolors();
#endif
    }
    idle_timer      = timer_read();
    halfmin_counter = 0;
}

static void update_led_timeout(void) {
    if (idle_timer == 0) idle_timer = timer_read();

    if (led_on && timer_elapsed(idle_timer) > 30000) {
        halfmin_counter++;
        idle_timer = timer_read();
    }

    if (led_on && halfmin_counter >= RGBMATRIX_TIMEOUT * 2) {
#ifdef RGB_MATRIX_ENABLE
        rgb_matrix_set_color_all(RGB_OFF);
#elif defined(RGBLIGHT_ENABLE)
        rgblight_sethsv_noeeprom(HSV_OFF);
#endif
        led_on          = false;
        halfmin_counter = 0;
    }
}

// 消灯中の最初の打鍵は点灯だけして、キーは送出しない
static bool process_led_timeout(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return true;

    const bool was_off = !led_on;
    refresh_led_timeout();
    return !was_off;
}

#endif // WINDMILL_LED_ENABLE


/*
 * 修飾キーのホールド中だけ英数レイヤーへ移す
 */

/* かな中でも Ctrl+C や Win+V がかな配列のまま解決されないよう、修飾キーを
 * ホールドしている間だけ英数レイヤーを上げる。Ctrl (MY_LCTL) だけだったのを
 * Win(つ) / Alt(さ) にも揃えた (issue #34)。
 *
 * レイヤーを上げている修飾を1ビットずつ覚える。QMKのレイヤーは参照カウントを
 * 持たないので、素のままだと Ctrl+Win の重ね押しで先に離したほうの layer_off()
 * が、まだ押している側のぶんまで落としてしまう。 */
#define HOLD_LAYER LAYER_ALPHA

static uint8_t hold_layer_mods      = 0;     // レイヤーを上げている修飾 (MOD_BIT)
static bool    hold_layer_ctrl_seen = false; // Ctrl がホストへ出たのを確認済みか (update_hold_layer 参照)

static void hold_layer_on(uint8_t mod) {
    if (!hold_layer_mods) layer_on(HOLD_LAYER);
    hold_layer_mods |= mod;
}

static void hold_layer_off(uint8_t mod) {
    if (!(hold_layer_mods & mod)) return;
    hold_layer_mods &= ~mod;
    if (mod == MOD_BIT(KC_LEFT_CTRL)) hold_layer_ctrl_seen = false;
    if (!hold_layer_mods) layer_off(HOLD_LAYER);
}

/* Ctrl (MY_LCTL) のぶんは、キーの解放イベントではなく**ホストへ出ている Ctrl** を
 * 毎スキャン見て下ろす。
 *
 * MY_LCTL は QMK の mod-tap で、Ctrl を上げ下げするのも、解放をどのキーコードで
 * 解決するのも QMK が持っている。以前は Win / Alt も mod-tap で、自分で英数レイヤーを
 * 上げたせいで解放が英数レイヤー側の素の KC_LGUI として引き直され、mod-tap の解放を
 * 待っていたレイヤーが貼りついたことがある。修飾の状態を見るなら、解放がどう
 * 解決されようと必ず戻る。Win / Alt は自前のホールド判定になった (issue #79) ので、
 * 解放をそのまま見ている。
 *
 * ただしレイヤーを上げた直後は、QMKがまだ Ctrl を register していないスキャンが
 * ありうる。一度ホストへ出たのを確認してから下ろす。 */
static void update_hold_layer(void) {
    if (!(hold_layer_mods & MOD_BIT(KC_LEFT_CTRL))) return;

    if (get_mods() & MOD_MASK_CTRL) {
        hold_layer_ctrl_seen = true;
    } else if (hold_layer_ctrl_seen) {
        hold_layer_off(MOD_BIT(KC_LEFT_CTRL));
    }
}

/*
 * MY_LCTL: tap = 言語切替 (英数⇔かな), hold = Ctrl + 英数レイヤー
 *
 * QMK標準の mod-tap (LCTL_T(KC_NO)、windmill.h 参照)。tapping term の計測も、
 * 別キー割り込みでのホールド確定 (HOLD_ON_OTHER_KEY_PRESS) も、Ctrl の
 * 上げ下げも QMK がやる。ここはタップ時の言語切替と、ホールド中だけ英数
 * レイヤーへ移すぶんだけを持つ。
 *
 * タップした直後 (QUICK_TAP_TERM = TAPPING_TERM 以内) に押し直すと、QMKは
 * ホールドではなくタップの繰り返しとみなす。MY_LCTL のタップ側は KC_NO で
 * 繰り返す中身が無いので、言語切替がもう一度走り、Ctrlは取れない。
 * get_quick_tap_term() で MY_LCTL だけ切れるが、そのために全機種へ config.h が
 * 要る。切り替えた直後にCtrlを押し続ける打ち方はしないので、素のままにする。
 */

/* 以前は 1回タップ = KC_LNG2 (英数)、2回タップ = KC_LNG1 (かな) と、モードを
 * 直接指定していた。Android でも KC_LNGx 自体は効くが、日本語IMEは入力欄を
 * 移るたびに既定のかなへ戻ってしまい、英数を保てない。そこでタップのたびに
 * 英数⇔かなを入れ替える形に改め、Android ではIMEそのものを切り替える
 * (issue #53)。ダブルタップを待つ必要が無いので、離した時点で確定する。
 *
 * キーボード側はIMEの状態を読めないので、ベースレイヤーは自分で反転させて追う。
 * 起動時はどちらも英数から始まる前提 (reset_default_layer 参照)。
 *
 * IMEへ送るキーはOSで違うので MY_WIN / MY_ANDR (接続先ごとの設定) で出し分ける。
 * - Windows: 反転後のベースレイヤーに合わせて KC_LNG1 (かな) / KC_LNG2 (英数) を
 *   交互に送る。IME側をマウスなどで切り替えてずれても、LNGx はモードを直接
 *   指定するので、もう一度タップすれば揃う。半角/全角のようなトグルのキーを
 *   送ると、ずれたまま戻せなくなる
 * - Android: Ctrl+Space で日本語IME⇔英語IMEを切り替える。英語IMEなら入力欄を
 *   移っても勝手にかなへ戻らない。日本語IMEは移るたびにかなへ戻るが、それは
 *   こちらのかなレイヤーと一致しているのでずれない。ただしトグルなので、IME側を
 *   別の手段で切り替えてずれた場合は、IME側を合わせ直すしかない
 *
 * Symレイヤーの数字・記号などで使う一時的な KC_LNG2 → キー → KC_LNG1 は、同じ
 * 入力欄の中で完結するので Android でもそのまま使える。 */
#define LANG_TOGGLE_ANDR C(KC_SPC)

/* ホストのIMEを layer (LAYER_KANA / LAYER_ALPHA) の側へ切り替える。
 * Android はモードを直接指定できないので layer は使わず、ただ切り替える */
static void send_lang_to_host(uint8_t layer) {
    if (is_android()) {
        tap_code16(LANG_TOGGLE_ANDR);
    } else {
        tap_code16(layer == LAYER_KANA ? KC_LNG1 : KC_LNG2);
    }
}

static bool is_kana_base(void) {
    return get_highest_layer(default_layer_state) == LAYER_KANA;
}

/* タップ確定。ベースレイヤーを反転させ、IMEもそちらへ切り替える。
 * default_layer_set はEEPROMを書かないので頻繁な切り替えでも安全 */
static void switch_lang(void) {
    const uint8_t next = is_kana_base() ? LAYER_ALPHA : LAYER_KANA;

    send_lang_to_host(next);
    default_layer_set((layer_state_t)1 << next);
}

/*
 * 文字のキーの送り方
 */

/* IMEがレポートを取りこぼさないためのウェイト。効かない場合は増やし、
 * もたつくなら減らす。
 *
 * 修飾の上げ下げも英数/かなの切り替えもIMEが非同期に処理するので、
 * レポートを続けて送ると前のぶんを取りこぼす。1本ごとに間を空けて
 * ホスト側での順序を保証する (issue #17、issue #36)。 */
#define IME_WAIT_MS 10

/* 英数レイヤー以外の文字のキーは、ここで1回だけ送る (issue #79)。output は出力表の
 * 値で、ホストに届くべき最終形 (S() 付きなら Shift 付きで届く)。押されている Shift は
 * 打ち消し、要るときだけ足す。
 *
 * | | Shift を押している | 押していない |
 * |--|--|--|
 * | 出力に Shift が要る | そのまま送る | Shift を足して送る |
 * | 出力に Shift が要らない | Shift を外して送る | そのまま送る |
 *
 * - Shift を押しているときに Shift 付きのキーを送るなら、押されている Shift をそのまま
 *   使い、修飾の付け外しを一切しない。以前は実Shiftを外して S() の weak Shift を付けて
 *   いたので、ホストには「右Shiftを離して左Shiftを押す」レポートが1本挟まっていた。
 *   この入れ替えが起きるのは親指Shiftを押してからの1打鍵目だけで、そこだけ Shift が
 *   効かない機種があった (issue #18)。親指Shiftは左に統一してある (issue #37) が、
 *   外付けキーボードの右Shiftと併用すれば同じ罠が復活するので、この形を保つ
 * - Shift を外して送るときは、外す前後にウェイトを挟む。挟まないと、親指Shiftの
 *   ホールドが確定した直後の1打鍵目だけ Shift が乗ったまま解釈されていた (issue #36)。
 *   ホールドの確定と Shift を外すレポートが同じスキャンで出るので、IMEが Shift の
 *   上げ下げを追い切れない。戻す前にも挟むのは、「こ」+「み」の半角スペースで
 *   そうしていたものに揃えたため
 * - 修飾は register / unregister で上げ下げする。del_mods() / set_mods() は real_mods を
 *   書き換えるだけでレポートを送らないので、ホスト側では Shift の上げ下げが
 *   見えなくなる (これも issue #18)
 *
 * ime_wrap なら、英数 (KC_LNG2) へ切り替えて送り、かな (KC_LNG1) へ戻す。かなベースの
 * 記号レイヤーと、Shift 面の半角「?」で使う。KC_LNG2 / KC_LNG1 に Shift が乗っていると
 * IMEが英数/かなキーとして受け取らないので、切り替えの間は必ず Shift を外す。外した
 * あとで Shift が要る文字を送るときは、外した実Shiftを戻して使う (weak Shift を
 * 付けると上の issue #18 の罠になる)。IMEのモードの切り替えも非同期なので、間に
 * ウェイトを挟まないと切り替え完了前にキーが届いて全角のまま入力される */
static void send_output(uint16_t output, bool ime_wrap) {
    const uint8_t shift      = get_mods() & MOD_MASK_SHIFT;
    const bool    need_shift = IS_QK_MODS(output) && (QK_MODS_GET_MODS(output) & MOD_LSFT);
    const uint8_t key        = IS_QK_MODS(output) ? QK_MODS_GET_BASIC_KEYCODE(output) : output;

    if (ime_wrap) {
        if (shift) {
            unregister_mods(shift);
            wait_ms(IME_WAIT_MS);
        }
        tap_code(KC_LNG2);
        wait_ms(IME_WAIT_MS);
        if (shift && need_shift) {
            register_mods(shift); // 押されていたShiftをそのまま戻して使う
            wait_ms(IME_WAIT_MS);
            tap_code(key);
            wait_ms(IME_WAIT_MS);
            unregister_mods(shift);
        } else {
            tap_code16(output); // Shift は外してあるので、要るなら weak Shift が付く
        }
        wait_ms(IME_WAIT_MS);
        tap_code(KC_LNG1);
        if (shift) {
            wait_ms(IME_WAIT_MS);
            register_mods(shift); // 押しっぱなしの実Shiftを戻す
        }
        return;
    }

    if (!shift) {
        tap_code16(output);
    } else if (need_shift) {
        tap_code(key); // 押されているShiftをそのまま使う
    } else {
        wait_ms(IME_WAIT_MS);
        unregister_mods(shift);
        wait_ms(IME_WAIT_MS);
        tap_code(key);
        wait_ms(IME_WAIT_MS);
        register_mods(shift);
    }
}

/*
 * 記号とかなのキーの出力表
 */

/* 記号とかなのキー (KN_* / KS_* / SY_*) は独自キーコードで、ホストへ送るキーはここの表で
 * 引く (issue #73)。同じ文字でも、送るべきキーコードは接続先のキーボード配列で
 * 変わる (「@」は US なら Shift+2、JIS なら単独のキー)。キーマップに素のキーコードを
 * 直接置くとそこを吸収できないので、キーマップは「どの文字のキーか」だけを持つ。
 *
 * 1キー1値で、値はホストに届くべき最終形 (issue #79)。Shift の有無まで値が決めるので、
 * 押されている Shift に左右されない (send_output 参照)。
 *
 * 基準は Windows + JIS。別の配列やOSで変わるものは、差分を例外 (key_output_exceptions[])
 * として持つ。基準を JIS にしたのは、かなはOSのIMEがキーの位置で決めるので、JIS の
 * 値ならJISかな配列の刻印そのままに書けるから。記号は keymap_japanese.h の JP_* で書く。
 * JP_AT は「JIS 配列で @ が出るキー」で、中身は KC_LBRC */
#define ROW(keycode) ((keycode) - KEY_OUTPUT_FIRST)

static const uint16_t PROGMEM key_outputs[] = {
    // かなレイヤー
    [ROW(KN_NU)]       = KC_1,
    [ROW(KN_FU)]       = KC_2,
    [ROW(KN_A)]        = KC_3,
    [ROW(KN_U)]        = KC_4,
    [ROW(KN_E)]        = KC_5,
    [ROW(KN_O)]        = KC_6,
    [ROW(KN_YA)]       = KC_7,
    [ROW(KN_YU)]       = KC_8,
    [ROW(KN_YO)]       = KC_9,
    [ROW(KN_WA)]       = KC_0,
    [ROW(KN_TA)]       = KC_Q,
    [ROW(KN_TE)]       = KC_W,
    [ROW(KN_I)]        = KC_E,
    [ROW(KN_SU)]       = KC_R,
    [ROW(KN_KA)]       = KC_T,
    [ROW(KN_N)]        = KC_Y,
    [ROW(KN_NA)]       = KC_U,
    [ROW(KN_NI)]       = KC_I,
    [ROW(KN_RA)]       = KC_O,
    [ROW(KN_SE)]       = KC_P,
    [ROW(KN_DAKU)]     = JP_AT,
    [ROW(KN_CHI)]      = KC_A,
    [ROW(KN_TO)]       = KC_S,
    [ROW(KN_SHI)]      = KC_D,
    [ROW(KN_HA)]       = KC_F,
    [ROW(KN_KI)]       = KC_G,
    [ROW(KN_KU)]       = KC_H,
    [ROW(KN_MA)]       = KC_J,
    [ROW(KN_NO)]       = KC_K,
    [ROW(KN_RI)]       = KC_L,
    [ROW(KN_RE)]       = JP_SCLN,
    [ROW(KN_KE)]       = JP_COLN,
    [ROW(KN_RU)]       = JP_DOT,
    [ROW(KN_ME)]       = JP_SLSH,
    [ROW(KN_RO)]       = JP_BSLS,

    // かなレイヤーの Shift 面
    [ROW(KN_XA)]       = S(KC_3),
    [ROW(KN_XU)]       = S(KC_4),
    [ROW(KN_XE)]       = S(KC_5),
    [ROW(KN_XO)]       = S(KC_6),
    [ROW(KN_XYA)]      = S(KC_7),
    [ROW(KN_XYU)]      = S(KC_8),
    [ROW(KN_XYO)]      = S(KC_9),
    [ROW(KN_WO)]       = S(KC_0),
    [ROW(KN_HE)]       = JP_CIRC,
    [ROW(KN_XI)]       = S(KC_E),
    [ROW(KN_MU)]       = JP_RBRC,
    [ROW(KN_HO)]       = JP_MINS,
    [ROW(KN_LKAKKO)]   = S(JP_LBRC),
    [ROW(KN_RKAKKO)]   = S(JP_RBRC),
    [ROW(KN_HANDAKU)]  = JP_LBRC,
    [ROW(KN_XTSU)]     = S(KC_Z),
    [ROW(KN_TOUTEN)]   = S(JP_COMM),
    [ROW(KN_KUTEN)]    = S(JP_DOT),
    [ROW(KN_NAKAGURO)] = S(JP_SLSH),
    [ROW(KN_CHOUON)]   = JP_YEN,

    // かなレイヤー最下段のタップ
    [ROW(KN_TSU)]      = KC_Z,
    [ROW(KN_SA)]       = KC_X,
    [ROW(KN_SO)]       = KC_C,
    [ROW(KN_HI)]       = KC_V,
    [ROW(KN_KO)]       = KC_B,
    [ROW(KN_MI)]       = KC_N,
    [ROW(KN_MO)]       = KC_M,
    [ROW(KN_NE)]       = JP_COMM,
    [ROW(KS_TSU)]      = S(KC_Z),
    [ROW(KS_KO)]       = KC_SPC,
    [ROW(KS_MI)]       = KC_SPC,
    [ROW(KS_MO)]       = JP_QUES,
    [ROW(KS_NE)]       = S(JP_COMM),

    // 記号レイヤー
    [ROW(SY_1)]        = KC_1,
    [ROW(SY_2)]        = KC_2,
    [ROW(SY_3)]        = KC_3,
    [ROW(SY_4)]        = KC_4,
    [ROW(SY_5)]        = KC_5,
    [ROW(SY_6)]        = KC_6,
    [ROW(SY_7)]        = KC_7,
    [ROW(SY_8)]        = KC_8,
    [ROW(SY_9)]        = KC_9,
    [ROW(SY_0)]        = KC_0,
    [ROW(SY_EXLM)]     = JP_EXLM,
    [ROW(SY_AT)]       = JP_AT,
    [ROW(SY_HASH)]     = JP_HASH,
    [ROW(SY_DLR)]      = JP_DLR,
    [ROW(SY_PERC)]     = JP_PERC,
    [ROW(SY_CIRC)]     = JP_CIRC,
    [ROW(SY_AMPR)]     = JP_AMPR,
    [ROW(SY_ASTR)]     = JP_ASTR,
    [ROW(SY_LPRN)]     = JP_LPRN,
    [ROW(SY_RPRN)]     = JP_RPRN,
    [ROW(SY_GRV)]      = JP_GRV,
    [ROW(SY_EQL)]      = JP_EQL,
    [ROW(SY_PLUS)]     = JP_PLUS,
    [ROW(SY_MINS)]     = JP_MINS,
    [ROW(SY_UNDS)]     = JP_UNDS,
    [ROW(SY_LBRC)]     = JP_LBRC,
    [ROW(SY_RBRC)]     = JP_RBRC,
    [ROW(SY_TILD)]     = JP_TILD,
    [ROW(SY_LCBR)]     = JP_LCBR,
    [ROW(SY_RCBR)]     = JP_RCBR,
    [ROW(SY_PIPE)]     = JP_PIPE,
    [ROW(SY_QUES)]     = JP_QUES,
};
_Static_assert(ARRAY_SIZE(key_outputs) == ROW(KEY_OUTPUT_LAST) + 1, "key_outputs[] の行が独自キーコードの数と合っていない");
_Static_assert(WINDMILL_KEYCODE_END - 1 <= QK_USER_MAX, "独自キーコードが QK_USER の範囲に収まっていない");

#undef ROW

#define OS_ANY 0xFF // 例外をどのOSにも当てる

/* 基準 (Windows + JIS) と違うものの例外。上から見て、キーと配列とOSが先に当たったものを
 * 使う。OSで分けたい行は、OS_ANY の行より上に置くこと。
 *
 * US 配列では、JISかな配列の刻印に無いキー (= ^ ] ¥ ろ) に載っているかなが、別の
 * キーへ逃がされている。「へ」は ^ の代わりに Shift+= (US の = の位置)、「む」は
 * ] の代わりに \ のキー、「ー」は ¥ の代わりに Shift+-、「ろ」は ` のキー。
 *
 * 「」はIMEによって載っている位置が違う。US 配列のとき、Windows (Microsoft IME) は
 * [ と ] のShiftで出るが、Android (Gboard) は ] と \ のShiftで出る。Android のほうは
 * JISかな配列の刻印どおりの位置なので、JIS では基準と同じになる (未確認) */
static const struct {
    uint16_t keycode;
    uint8_t  layout; // WINDMILL_LAYOUT_*
    uint8_t  os;     // OS_* か OS_ANY
    uint16_t output;
} PROGMEM key_output_exceptions[] = {
    // かな
    {KN_RO, WINDMILL_LAYOUT_US, OS_ANY, KC_GRV},
    {KN_HE, WINDMILL_LAYOUT_US, OS_ANY, S(KC_EQL)},
    {KN_MU, WINDMILL_LAYOUT_US, OS_ANY, KC_BSLS},
    {KN_LKAKKO, WINDMILL_LAYOUT_US, OS_ANDR, S(KC_RBRC)},
    {KN_LKAKKO, WINDMILL_LAYOUT_US, OS_ANY, S(KC_LBRC)},
    {KN_RKAKKO, WINDMILL_LAYOUT_US, OS_ANDR, S(KC_BSLS)},
    {KN_RKAKKO, WINDMILL_LAYOUT_US, OS_ANY, S(KC_RBRC)},
    {KN_CHOUON, WINDMILL_LAYOUT_US, OS_ANY, S(KC_MINS)},

    // 記号
    {SY_AT, WINDMILL_LAYOUT_US, OS_ANY, S(KC_2)},
    {SY_CIRC, WINDMILL_LAYOUT_US, OS_ANY, S(KC_6)},
    {SY_AMPR, WINDMILL_LAYOUT_US, OS_ANY, S(KC_7)},
    {SY_ASTR, WINDMILL_LAYOUT_US, OS_ANY, S(KC_8)},
    {SY_LPRN, WINDMILL_LAYOUT_US, OS_ANY, S(KC_9)},
    {SY_RPRN, WINDMILL_LAYOUT_US, OS_ANY, S(KC_0)},
    {SY_GRV, WINDMILL_LAYOUT_US, OS_ANY, KC_GRV},
    {SY_EQL, WINDMILL_LAYOUT_US, OS_ANY, KC_EQL},
    {SY_PLUS, WINDMILL_LAYOUT_US, OS_ANY, S(KC_EQL)},
    {SY_UNDS, WINDMILL_LAYOUT_US, OS_ANY, S(KC_MINS)},
    {SY_LBRC, WINDMILL_LAYOUT_US, OS_ANY, KC_LBRC},
    {SY_RBRC, WINDMILL_LAYOUT_US, OS_ANY, KC_RBRC},
    {SY_TILD, WINDMILL_LAYOUT_US, OS_ANY, S(KC_GRV)},
    {SY_LCBR, WINDMILL_LAYOUT_US, OS_ANY, S(KC_LBRC)},
    {SY_RCBR, WINDMILL_LAYOUT_US, OS_ANY, S(KC_RBRC)},
    {SY_PIPE, WINDMILL_LAYOUT_US, OS_ANY, S(KC_BSLS)},
};

/* 出力表を引く配列。まだ配列の切り替えを有効にしていないので、設定 (MY_JIS / MY_US) に
 * 依らず US を引く。英数レイヤーの記号や LT() のタップ側のような素のキーコードは
 * US 配列向けのままで、ここだけ JIS にすると文字が混ざる。切り替えの有効化は
 * issue #75 で、そこで windmill_host_layout() に替える */
static uint8_t key_output_layout(void) {
    return WINDMILL_LAYOUT_US;
}

// keycode は KEY_OUTPUT_FIRST 〜 KEY_OUTPUT_LAST であること
static uint16_t get_key_output(uint16_t keycode) {
    const uint8_t layout = key_output_layout();
    const uint8_t os     = is_android() ? OS_ANDR : OS_WIN; // 予約の値は Windows と同じに扱う

    for (uint8_t i = 0; i < ARRAY_SIZE(key_output_exceptions); ++i) {
        if (pgm_read_word(&key_output_exceptions[i].keycode) != keycode) continue;
        if (pgm_read_byte(&key_output_exceptions[i].layout) != layout) continue;
        const uint8_t row_os = pgm_read_byte(&key_output_exceptions[i].os);
        if (row_os != OS_ANY && row_os != os) continue;
        return pgm_read_word(&key_output_exceptions[i].output);
    }
    return pgm_read_word(&key_outputs[keycode - KEY_OUTPUT_FIRST]);
}

static bool is_key_output_target(uint16_t keycode) {
    return keycode >= KEY_OUTPUT_FIRST && keycode <= KEY_OUTPUT_LAST;
}

uint16_t windmill_output_keycode(uint16_t keycode) {
    return is_key_output_target(keycode) ? get_key_output(keycode) : keycode;
}

/* 英数へ切り替えて送るキーか。かなベースの記号レイヤーは、かな入力のままでは全角に
 * なるので1打鍵ごとに英数へ切り替える。Shift 面の「?」も、かな入力のままでは打てない
 * 半角の記号なので同じ (issue #17)。
 *
 * 記号レイヤーのキーは記号レイヤー専用の独自キーコードなので、かなレイヤーの
 * キーを巻き込む心配が無い */
static bool is_ime_wrap_target(uint16_t keycode) {
    if (!is_kana_base()) return false;
    return (keycode >= SY_1 && keycode <= SY_QUES) || keycode == KS_MO;
}

static void send_key_output(uint16_t keycode) {
    send_output(get_key_output(keycode), is_ime_wrap_target(keycode));
}

/* 出力表で引くキー (最下段のタップとホールドを持つものを除く) を送る。常に false
 * (QMKには渡さない)。押下で送り切り、解放では何もしない (issue #79)。
 *
 * 機種側の process_record にも、ホストへ送るキーコードとして渡す。geonix41 のブロブは
 * キーマップに素のキーコードを直接置いていたころからこれらのキーを見ている */
static bool process_output_key(uint16_t keycode, keyrecord_t *record) {
    if (!windmill_board_process_record(get_key_output(keycode), record)) return false;

    if (record->event.pressed) {
        send_key_output(keycode);
    }
    return false;
}

/*
 * 英数レイヤーの親指Shift (SandS) のハンドオーバー
 */

/* 左右とも ALPHA_THUMB_SHIFT (LSFT_T(KC_SPC)) で同じキーコードなので、
 * キーコードでは左右を区別できない。windmill.h の説明どおり matrix の列 (col) で
 * 見分ける (issue #40)。列は機種ごとに違いうるので、値は windmill.h / 機種の
 * config.h が持つ (issue #68)。
 *
 * QMKの修飾ビットは左右で共有される。参照カウントが無いため、素のままだと先に
 * 離したほうの unregister_mods() で Shift が落ち、もう片方を押していてもホストからは
 * Shift が離れて見える。押しっぱなしのほうが残っているうちは離すイベントごと消費して、
 * 持ち替えを途切れさせない。Shift+タップ (半角スペース) はそのままQMKに任せてよい。 */
static uint8_t alpha_thumb_shift_held = 0;

#define ALPHA_THUMB_SHIFT_BIT(record) ((record)->event.key.col == ALPHA_THUMB_SHIFT_L_COL ? 1 : 2)

// falseを返したらそのイベントは消費済み(以降の処理をスキップ)
static bool process_alpha_thumb_shift(keyrecord_t *record) {
    if (record->tap.count) return true; // タップ(スペース)はQMKに任せる

    if (record->event.pressed) { // hold確定 = Shift開始
        alpha_thumb_shift_held |= ALPHA_THUMB_SHIFT_BIT(record);
    } else { // hold解放
        alpha_thumb_shift_held &= ~ALPHA_THUMB_SHIFT_BIT(record);
        // もう片方がまだShiftとして押されているなら、QMKに離させない
        if (alpha_thumb_shift_held) return false;
    }
    return true; // 通常のtap(スペース)とholdはQMKに任せる
}

/*
 * 接続先ごとの設定
 */

// いま繋がっている接続先のOSだけを書き換える。他の接続先の設定は触らない
static void set_host_os(uint8_t os) {
    const uint8_t  shift = current_host() * OS_BITS;
    const uint16_t next  = (windmill_config.host_os & ~(OS_MASK << shift)) | ((os & OS_MASK) << shift);
    if (windmill_config.host_os == next) return; // 無変更ならEEPROMを書かない
    windmill_config.host_os = next;
    eeconfig_update_kb(windmill_config.raw);
}

// 配列も同じく、いま繋がっている接続先のぶんだけを書き換える
static void set_host_layout(uint8_t layout) {
    const uint8_t  shift = current_host() * LAYOUT_BITS;
    const uint32_t next  = (windmill_config.host_layout & ~((uint32_t)LAYOUT_MASK << shift)) | ((uint32_t)(layout & LAYOUT_MASK) << shift);
    if (windmill_config.host_layout == next) return; // 無変更ならEEPROMを書かない
    windmill_config.host_layout = next;
    eeconfig_update_kb(windmill_config.raw);
}

/*
 * Android での Win / Alt
 */

/* Android は Win (Meta) と Alt を特別扱いする (issue #57)。効いてくるのは次の2つで、
 * どちらもキーそのものではなく押し離しの順序で決まる (AOSP の
 * PhoneWindowManager / KeyGestureController)。
 *
 * - Win と Alt の片方を押している間にもう片方を押すと、どちらかを離した時点で
 *   Caps Lock が切り替わる。間に他のキーを挟めば取り消される
 * - Win を押して、他のキーを何も挟まずに離すと、アプリ一覧が開く
 *
 * かなレイヤーの「つ」「さ」は隣り合った Win / Alt なので、片方をホールドしたまま
 * もう片方を打つと前者を踏む。片方のホールドで英数レイヤーが上がり、もう片方は
 * 英数レイヤーの素の KC_LGUI / KC_LALT になるため。 */

/* 押下を握りつぶした修飾 (MOD_BIT)。離すイベントも消費するために覚える。
 * 握りつぶしたほうは、先に押したほうを離しても出さない。出すと押した覚えの無い
 * 修飾が遅れて効くことになる */
static uint8_t android_blocked_mods = 0;

/* Win と Alt は後から押したほうをホストへ出さない。mod (MOD_BIT) の押下を
 * 握りつぶしたら true */
static bool android_block_press(uint8_t mod) {
    const uint8_t other = (mod == MOD_BIT(KC_LEFT_GUI)) ? MOD_MASK_ALT : MOD_MASK_GUI;
    if (!is_android() || !(get_mods() & other)) return false;

    android_blocked_mods |= mod;
    return true;
}

/* 握りつぶした押下に対応する解放なら true。押下時のOS設定で決まるので、押している間に
 * 設定を変えても離すぶんは消費する */
static bool android_block_release(uint8_t mod) {
    if (!(android_blocked_mods & mod)) return false;

    android_blocked_mods &= ~mod;
    return true;
}

/* 英数レイヤーの素の KC_LGUI / KC_LALT。falseを返したらそのイベントは消費済み
 * (以降の処理をスキップ)。修飾そのものはQMKに任せる。
 *
 * かなベースで英数レイヤーが上がっているとき (Ctrl などを先に押したとき) は、
 * つ / さ の位置がこれになる。これも英数レイヤーを上げている修飾として数えないと、
 * Ctrl を先に離した時点でレイヤーが落ちてしまう。押下を握りつぶしたときは
 * レイヤーも上げない */
static bool process_alpha_gui_alt(uint16_t keycode, keyrecord_t *record) {
    const uint8_t mod = (keycode == KC_LGUI) ? MOD_BIT(KC_LEFT_GUI) : MOD_BIT(KC_LEFT_ALT);

    if (record->event.pressed) {
        if (android_block_press(mod)) return false;
        if (is_kana_base()) hold_layer_on(mod);
    } else {
        if (android_block_release(mod)) return false;
        hold_layer_off(mod);
    }
    return true;
}

/* Win を押したまま、Android に「Win を単独で押して離した」と見なされないようにする。
 * 他のキーを1つ挟めば取り消されるので、何も起こさないキーを空打ちする。
 * 右Ctrlにしたのは、このキーマップに無く (Ctrl は左だけ)、Win との組み合わせに
 * ショートカットが無いため。QMK の DUMMY_MOD_NEUTRALIZER_KEYCODE でも
 * 推奨されている */
static void neutralize_gui_tap(void) {
    tap_code(KC_RIGHT_CTRL);
}

// Win+. を Alt+. に読み替えて送ったあとの、ピリオドの解放待ち
static bool android_gui_dot_sent = false;

/* Win+. は Alt+. として送る。Windows の Win+. (絵文字) に揃えるため。
 *
 * Win を Alt に差し替えるには Win をいったん離すしかない (重ねると Caps Lock)。
 * ただ離すだけだとアプリ一覧が開いてしまうので、離す前に空打ちを挟む。
 * 押し直したあとも同じで、挟まないと指を離したときに開く。
 *
 * 押下の中で送り切るのは、Win を外したままスキャンを跨がないため。跨ぐと、その間に
 * 打った別のキーに Win が乗らない。
 * falseを返したらそのイベントは消費済み(以降の処理をスキップ) */
static bool process_android_gui_dot(keyrecord_t *record) {
    if (!record->event.pressed) {
        if (!android_gui_dot_sent) return true;
        android_gui_dot_sent = false;
        return false; // 押下で送り切ったので、未registerのunregisterを防ぐ
    }

    const uint8_t gui = get_mods() & MOD_MASK_GUI;
    if (!is_android() || !gui) return true;

    neutralize_gui_tap();
    unregister_mods(gui);
    tap_code16(A(KC_DOT));
    register_mods(gui);
    neutralize_gui_tap();

    android_gui_dot_sent = true;
    return false;
}

/*
 * Fn と設定レイヤー
 */

/* 左右のFnを両方ホールドしている間だけ設定レイヤーへ移す (issue #62)。
 *
 * Fnレイヤーの Fn の位置へ MO(LAYER_CONF) を置けばコードは要らないが、それだと
 * 設定レイヤーを上げているのは後から押したほうのFnだけになる。先に押したほうを
 * 離しても設定レイヤーが残り、「両方押している間だけ」にならない。そこでホールド中の
 * Fnを左右1ビットずつ覚えて、レイヤーの上げ下げを自前で持つ。
 *
 * QMKのレイヤーは参照カウントを持たないので、Fnレイヤーのほうも左右のどちらかが
 * 残っているうちは下ろさない。英数レイヤーの Fn は QMK の MO() のままなので、
 * 片方が残っているうちは離すイベントごと消費する (process_alpha_fn)。かなレイヤーの
 * Fn (そ / ね) は自前のホールド判定なので、Fnレイヤーも自分で上げ下げする。
 *
 * 設定レイヤーの Fn の位置は必ず透過にしておくこと。英数レイヤーの2つ目の Fn の押下で
 * ここがレイヤーを上げると、QMKはそのキー自身のアクションを上げた後のレイヤーで
 * 引き直す。透過でないと押下も解放も MO() として解決されず、Fnレイヤーへ
 * 貼りついたまま戻れなくなる。 */
static uint8_t fn_held = 0;

#define FN_L    1
#define FN_R    2
#define FN_BOTH (FN_L | FN_R)
#define FN_BIT(record) ((record)->event.key.col == FN_L_COL ? FN_L : FN_R)

static void update_conf_layer(void) {
    if (fn_held == FN_BOTH) {
        layer_on(LAYER_CONF);
    } else if (layer_state_is(LAYER_CONF)) {
        layer_off(LAYER_CONF);
    }
}

// 英数レイヤーの Fn (MO)。falseを返したらそのイベントは消費済み(以降の処理をスキップ)
static bool process_alpha_fn(keyrecord_t *record) {
    if (record->event.pressed) {
        fn_held |= FN_BIT(record);
    } else {
        fn_held &= ~FN_BIT(record);
    }
    update_conf_layer();

    // もう片方がまだFnとして押されているなら、QMKにFnレイヤーを下ろさせない
    if (!record->event.pressed && fn_held) return false;
    return true; // Fnレイヤーの上げ下げはQMKに任せる
}

// かなレイヤーの Fn (そ / ね) のホールド
static void kana_fn_on(uint8_t bit) {
    fn_held |= bit;
    layer_on(LAYER_FN);
    update_conf_layer();
}

static void kana_fn_off(uint8_t bit) {
    fn_held &= ~bit;
    update_conf_layer();
    if (!fn_held) layer_off(LAYER_FN);
}

/*
 * かなレイヤー最下段のタップとホールド
 */

/* かなレイヤー最下段の8キー (つ さ そ ひ こ み も ね) は、タップでかなを出し、
 * ホールドで修飾やレイヤーになる。QMK の LT() / MT() はタップ側に8bitの基本キーコード
 * しか置けず、出力表を引けないので、判定を自前で持つ (issue #79)。
 *
 * - 判定は別のキーの押下時に行う。押されてから KANA_HOLD_TERM 以上ならホールド、
 *   未満ならタップ。タップならその場でかなを出す
 * - 別のキーの押下が無いまま離されたらタップ。長押ししてもタップになる
 * - ホールドの開始は別のキーの前処理 (pre_process_record_kb) で行う。QMK がそのキーの
 *   キーコードを解決するのはその後なので、別のキーは上げたレイヤーで解決される
 * - キーを溜めて流し直す仕組みは持たない。別のキーの押下で必ず決まるので、決まって
 *   いないのは常に高々1つ

 *
 * 英数レイヤーのキーと Ctrl (MY_LCTL) は QMK の LT() / MT() のまま。 */
#ifndef KANA_HOLD_TERM
#    define KANA_HOLD_TERM 150
#endif

// ホールドの役割を持つキーの番号。windmill.h の KN_TSU 〜 KN_NE の並びと同じ
enum {
    KH_TSU, // Win。ホールド中は英数レイヤー (issue #34)
    KH_SA,  // Alt。同上
    KH_SO,  // Fn (左)。左右両方で設定レイヤー (issue #62)
    KH_HI,  // Sym
    KH_KO,  // Shift (左) と Shift 面
    KH_MI,  // Shift (右) と Shift 面
    KH_MO,  // Sym
    KH_NE,  // Fn (右)
    KH_NONE = 0xFF,
};

#define KH_BIT(index) (1u << (index))
#define KH_SHIFT_BITS (KH_BIT(KH_KO) | KH_BIT(KH_MI))
#define KH_SYM_BITS   (KH_BIT(KH_HI) | KH_BIT(KH_MO))

/* Shift 面の最下段 (KS_*) は、かなレイヤーの同じ位置のキーとホールドの役割が同じ。
 * Shift 面にいる間に押したキーは、離すときも Shift 面のキーコードで来る (QMK が押下時の
 * レイヤーを覚えている) ので、役割さえ揃えておけば Shift 面を出入りしても食い違わない */
static uint8_t kana_hold_index(uint16_t keycode) {
    switch (keycode) {
        case KN_TSU ... KN_NE:
            return keycode - KN_TSU;
        case KS_TSU:
            return KH_TSU;
        case KS_KO:
            return KH_KO;
        case KS_MI:
            return KH_MI;
        case KS_MO:
            return KH_MO;
        case KS_NE:
            return KH_NE;
    }
    return KH_NONE;
}

static uint8_t  kana_pending = KH_NONE; // 押されていて、タップかホールドかまだ決まっていないキー
static uint16_t kana_pending_keycode;   // そのキーコード。タップならこれを出す
static uint16_t kana_pending_time;      // そのキーが押された時刻
static uint8_t  kana_holding = 0;       // ホールドになって、まだ押されているキー (KH_BIT)

/* Shift 面の こ / み をタップすると半角スペースを出す。ただし Shift 開始後に他のキーを
 * 押していたら、誤入力ガードとして何も出さない。Shift(親指ホールド)開始後に他のキーが
 * 押されたらダーティ */
static bool thumb_shift_dirty = false;

static bool is_thumb_shift_key(uint16_t keycode) {
    return keycode == KN_KO || keycode == KN_MI || keycode == KS_KO || keycode == KS_MI;
}

/* Win / Alt。ホールド中は英数レイヤーへ移す (issue #34)。Android で握りつぶした
 * ときは、修飾もレイヤーも動かさない */
static void kana_mod_on(uint8_t mod) {
    if (android_block_press(mod)) return;
    register_mods(mod);
    hold_layer_on(mod);
}

static void kana_mod_off(uint8_t mod) {
    if (android_block_release(mod)) return;
    unregister_mods(mod);
    hold_layer_off(mod);
}

/* 左右の Shift / Sym / Fn は、同じ役割のキーが1つでもホールドされていれば有効にする
 * (参照カウント)。QMKの修飾ビットもレイヤーも左右で共有されるので、素のままだと
 * 先に離したほうが、まだ押している側のぶんまで落としてしまう (issue #40、#62) */
static void kana_hold_start(uint8_t index) {
    const uint8_t others = kana_holding;
    kana_holding |= KH_BIT(index);

    switch (index) {
        case KH_TSU:
            kana_mod_on(MOD_BIT(KC_LEFT_GUI));
            break;
        case KH_SA:
            kana_mod_on(MOD_BIT(KC_LEFT_ALT));
            break;
        case KH_SO:
            kana_fn_on(FN_L);
            break;
        case KH_NE:
            kana_fn_on(FN_R);
            break;
        case KH_HI:
        case KH_MO:
            if (!(others & KH_SYM_BITS)) layer_on(LAYER_SYM);
            break;
        case KH_KO:
        case KH_MI:
            thumb_shift_dirty = false;
            if (!(others & KH_SHIFT_BITS)) {
                /* 左右とも左Shiftにする (issue #37)。S() の weak Shift は必ず左Shiftなので、
                 * 右Shiftだと修飾が入れ替わって見える罠がある (issue #18) */
                register_mods(MOD_BIT(KC_LEFT_SHIFT));
                layer_on(LAYER_KANA_SHIFT);
            }
            break;
    }
}

static void kana_hold_end(uint8_t index) {
    kana_holding &= ~KH_BIT(index);

    switch (index) {
        case KH_TSU:
            kana_mod_off(MOD_BIT(KC_LEFT_GUI));
            break;
        case KH_SA:
            kana_mod_off(MOD_BIT(KC_LEFT_ALT));
            break;
        case KH_SO:
            kana_fn_off(FN_L);
            break;
        case KH_NE:
            kana_fn_off(FN_R);
            break;
        case KH_HI:
        case KH_MO:
            if (!(kana_holding & KH_SYM_BITS)) layer_off(LAYER_SYM);
            break;
        case KH_KO:
        case KH_MI:
            if (!(kana_holding & KH_SHIFT_BITS)) {
                unregister_mods(MOD_BIT(KC_LEFT_SHIFT));
                layer_off(LAYER_KANA_SHIFT);
            }
            break;
    }
}

static void kana_hold_tap(uint16_t keycode) {
    if ((keycode == KS_KO || keycode == KS_MI) && thumb_shift_dirty) return; // ダーティ時は何も出さない
    send_key_output(keycode);
}

/* 別のキーが押された。決まっていないキーがあれば、押されてからの時間でタップか
 * ホールドかを決める。時刻はキーイベントのものを使い、スキャンの遅れに左右されない */
static void kana_hold_decide(uint16_t time) {
    if (kana_pending == KH_NONE) return;

    const uint8_t index = kana_pending;
    kana_pending        = KH_NONE;
    if (TIMER_DIFF_16(time, kana_pending_time) >= KANA_HOLD_TERM) {
        kana_hold_start(index);
    } else {
        kana_hold_tap(kana_pending_keycode);
    }
}

// 常に false (QMKには渡さない)
static bool process_kana_hold(uint8_t index, uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        kana_pending         = index;
        kana_pending_keycode = keycode;
        kana_pending_time    = record->event.time;
    } else if (kana_pending == index) { // 別のキーを押さずに離した
        kana_pending = KH_NONE;
        kana_hold_tap(kana_pending_keycode);
    } else if (kana_holding & KH_BIT(index)) {
        kana_hold_end(index);
    }
    // タップに決まったあとの解放は何もしない
    return false;
}

/*
 * QMK callbacks
 */

bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    windmill_board_pre_process_record(keycode, record);

    // かなレイヤー最下段のタップかホールドかは、別のキーの押下で決める
    if (record->event.pressed) {
        kana_hold_decide(record->event.time);
    }
    return pre_process_record_user(keycode, record);
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
#ifdef WINDMILL_LED_ENABLE
    if (!process_led_timeout(keycode, record)) return false;
#endif
    if (!process_record_user(keycode, record)) return false;

    // 親指Shift以外のキー押下でダーティ化
    if (record->event.pressed && !is_thumb_shift_key(keycode)) {
        thumb_shift_dirty = true;
    }

    const uint8_t hold = kana_hold_index(keycode);
    if (hold != KH_NONE) { // かなレイヤー最下段
        return process_kana_hold(hold, keycode, record);
    }
    if (is_key_output_target(keycode)) { // 記号とかなのキー
        return process_output_key(keycode, record);
    }

    switch (keycode) {
#ifdef WINDMILL_LED_ENABLE
        case MY_DARK: // LEDの明るさ 強/弱
            if (record->event.pressed) {
                toggle_darkmode();
            }
            return false;
#endif

        case MY_WIN:
            if (record->event.pressed) {
                set_host_os(OS_WIN);
            }
            return false;

        case MY_ANDR:
            if (record->event.pressed) {
                set_host_os(OS_ANDR);
            }
            return false;

        case MY_JIS:
            if (record->event.pressed) {
                set_host_layout(WINDMILL_LAYOUT_JIS);
            }
            return false;

        case MY_US:
            if (record->event.pressed) {
                set_host_layout(WINDMILL_LAYOUT_US);
            }
            return false;

        /* Fn+Ctrl: ホスト側のIMEだけ切り替える。ベースレイヤーは動かさない。
         *
         * IMEの状態はキーボードからは読めないので、IME側だけが切り替わるとずれる。
         * Windows は MY_LCTL のタップが KC_LNG1/KC_LNG2 でモードを直接指定するため
         * タップし直せば揃うが、Android は Ctrl+Space のトグルしか無く、
         * キーボードとIMEが同じ向きに動いてしまってずれたままになる。
         * こちらはキーボードを動かさずにIMEだけを動かすので、ずれを解消できる。
         *
         * 送るのは今のベースレイヤーに合わせたほう。Windows ではモードの直接指定に
         * なるので、ずれていなければ何も起きず、ずれていれば揃う。Android は
         * トグルなので、揃っている状態で押すと逆にずれる (もう一度押せば戻る) */
        case MY_IME:
            if (record->event.pressed) {
                send_lang_to_host(get_highest_layer(default_layer_state));
            }
            return false;

        case KC_DOT: // 英数レイヤーの「.」。Android では Win+. を Alt+. に読み替える (issue #57)
            if (!process_android_gui_dot(record)) {
                return false;
            }
            break;

        case KC_LGUI: // 英数レイヤーの Win / Alt (かなの つ / さ と同じ位置)
        case KC_LALT:
            if (!process_alpha_gui_alt(keycode, record)) {
                return false;
            }
            break; // 修飾そのものはQMKに任せる

        case ALPHA_FN: // 英数レイヤーの Fn (左右とも同じキーコード)
            if (!process_alpha_fn(record)) {
                return false;
            }
            break; // Fnレイヤーの上げ下げはQMKに任せる

        case ALPHA_THUMB_SHIFT: // 英数レイヤーの親指Shift (issue #40)
            if (!process_alpha_thumb_shift(record)) {
                return false;
            }
            break; // 通常処理へ (tap=スペース, hold=Shift)

        case MY_LCTL: // 言語切替 (タップ) / Ctrl + 英数レイヤー (ホールド)
            if (record->tap.count) {
                if (record->event.pressed) {
                    switch_lang();
                }
                return false; // タップ側は KC_NO なのでQMKへ渡さない
            }
            if (record->event.pressed) {
                hold_layer_on(MOD_BIT(KC_LEFT_CTRL)); // Ctrl自体はQMKのmod-tapが押す
            }
            break; // ホールドはQMKに任せる。レイヤーを下ろすのは update_hold_layer
    }

    return windmill_board_process_record(keycode, record);
}

void matrix_scan_kb(void) {
#ifdef WINDMILL_LED_ENABLE
    update_led_timeout();
#endif

    update_hold_layer();

    matrix_scan_user();
}

void keyboard_post_init_kb(void) {
    reset_default_layer();

    windmill_config.raw = eeconfig_read_kb();
    if (windmill_config.legacy_android) {
        /* 以前は接続先を区別していなかったので、どの接続先もAndroidだったことになる。
         * 移行は一度きりで、以後は legacy_android が立たない */
        windmill_config.legacy_android = false;
        for (uint8_t host = 0; host < WINDMILL_HOST_SIZE; ++host) {
            windmill_config.host_os |= OS_ANDR << (host * OS_BITS);
        }
        eeconfig_update_kb(windmill_config.raw);
    }
#ifdef WINDMILL_LED_ENABLE
    led_darkmode = windmill_config.led_darkmode;
#endif

    // keymap.c 側で windmill_init_keycolors() を呼ぶので、配色を組む前に済ませる
    keyboard_post_init_user();

#ifdef WINDMILL_LED_ENABLE
#    ifdef RGB_MATRIX_ENABLE
    rgb_matrix_mode_noeeprom(RGB_MATRIX_NONE); // アニメーションなし。全て自前で描く
    rgb_matrix_sethsv_noeeprom(HSV_OFF);
#    elif defined(RGBLIGHT_ENABLE)
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_sethsv_noeeprom(HSV_OFF);
#    endif

    cache_keycolors();
    led_initialized = true;
    refresh_keycolors(layer_state, default_layer_state);
#endif // WINDMILL_LED_ENABLE

    windmill_board_post_init();
}

#ifdef WINDMILL_LED_ENABLE

layer_state_t layer_state_set_kb(layer_state_t state) {
    state = layer_state_set_user(state);
    refresh_keycolors(state, default_layer_state);
    return state;
}

// MY_LCTL でベースレイヤー (かな/英数) が変わるので、こちらでも塗り直す
layer_state_t default_layer_state_set_kb(layer_state_t state) {
    state = default_layer_state_set_user(state);
    refresh_keycolors(layer_state, state);
    return state;
}

#    ifdef RGB_MATRIX_ENABLE
bool rgb_matrix_indicators_kb(void) {
    if (!rgb_matrix_indicators_user()) return false;

    // ベンダー実装の描画を先に走らせる。配色はこの上から塗る
    windmill_board_led_begin();

    if (!led_on || !led_initialized) return true;

    apply_keycolors();
    return true;
}
#    endif

#endif // WINDMILL_LED_ENABLE
