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
 * default_layer_set を使う。ただしかなレイヤー最下段のタップ・ホールドは自前で
 * 判定する (「かなレイヤー最下段のタップ・ホールド」参照)。かな入力はOS側の
 * IMEに任せる (レイヤー0が素の QWERTY+数字段で、標準のJISかな配置になる)。
 *
 * LEDはキーコードの種類ごとに色を割り当てる独自実装で、こちらは従来のまま。
 * LED非搭載機 (minipeg48) では WINDMILL_LED_ENABLE が立たず、配色処理は
 * 丸ごとコンパイルから外れる。 */

#include "windmill.h"

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

/* 接続先のOS。言語切替 (MY_LCTL) のキーと、かなレイヤーの「」(KN_RA / KN_SE の
 * Shift時出力) はOSのIME実装で必要なキーが異なるため、MY_WIN / MY_ANDR で切り替える。
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
 * が、まだ押している側のぶんまで落としてしまう。
 *
 * 上げ下げの契機は修飾ごとに違う。Ctrl は QMK の mod-tap なので、ホストへ出ている
 * 修飾を毎スキャン見て下ろす (update_hold_layer)。Win / Alt は、かなレイヤーの
 * つ / さ なら windmill.c 自身がホールドを出し入れするので、そこで上げ下げする
 * (apply_bottom_hold)。英数レイヤー側の素の KC_LGUI / KC_LALT は押下と解放で
 * 上げ下げする (process_alpha_gui_alt)。 */
#define HOLD_LAYER LAYER_ALPHA

static uint8_t hold_layer_mods      = 0;     // レイヤーを上げている修飾 (MOD_BIT)
static bool    hold_layer_ctrl_seen = false; // Ctrl がホストへ出たのを確認済み

static void hold_layer_on(uint8_t mod) {
    if (!hold_layer_mods) layer_on(HOLD_LAYER);
    hold_layer_mods |= mod;
}

static void hold_layer_off(uint8_t mod) {
    if (!(hold_layer_mods & mod)) return; // 上げていない修飾の解放 (英数ベースで押した KC_LGUI など)
    hold_layer_mods &= ~mod;
    if (mod & MOD_MASK_CTRL) hold_layer_ctrl_seen = false;
    if (!hold_layer_mods) layer_off(HOLD_LAYER);
}

/*
 * MY_LCTL: tap = 言語切替 (英数⇔かな), hold = Ctrl + 英数レイヤー
 *
 * QMK標準の mod-tap (LCTL_T(KC_NO)、windmill.h 参照)。tapping term の計測も、
 * 別キー割り込みでのホールド確定 (HOLD_ON_OTHER_KEY_PRESS) も、Ctrl の
 * 上げ下げも QMK がやる。ここはタップ時の言語切替と、ホールド中だけ英数
 * レイヤーへ移すぶんだけを持つ。かなレイヤーの Win(つ) / Alt(さ) と違い、
 * 判定は QMK のまま (issue #85)。
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

/* タップ確定。ベースレイヤーを反転させ、IMEもそちらへ切り替える。
 * default_layer_set はEEPROMを書かないので頻繁な切り替えでも安全 */
static void switch_lang(void) {
    const uint8_t next = get_highest_layer(default_layer_state) == LAYER_KANA ? LAYER_ALPHA : LAYER_KANA;

    send_lang_to_host(next);
    default_layer_set((layer_state_t)1 << next);
}

/*
 * Shift時に別の記号を出すキー
 */

/* IMEがレポートを取りこぼさないためのウェイト。効かない場合は増やし、
 * もたつくなら減らす。
 *
 * 修飾の上げ下げも英数/かなの切り替えもIMEが非同期に処理するので、
 * レポートを続けて送ると前のぶんを取りこぼす。1本ごとに間を空けて
 * ホスト側での順序を保証する (issue #17、issue #36)。 */
#define IME_WAIT_MS 10

// shifted 側が Shift 付きのキーコード (S(KC_x)) かどうか
static bool is_shifted_keycode(uint16_t keycode) {
    return IS_QK_MODS(keycode) && (QK_MODS_GET_MODS(keycode) & MOD_LSFT);
}

/* Shift付きで押されたら shifted を、そうでなければ plain を送出する
 * (QMKのkey override相当)。
 *
 * shifted 側もShiftを要するキー (S(KC_COMM) → 「、」など) では、押されている
 * Shiftをそのまま使い、修飾の付け外しを一切しない。以前は必ず
 *   実Shiftを外す → shifted のweak Shiftを付ける → 外す
 * としていたので、ホストには「右Shiftを離して左Shiftを押す」レポートが1本
 * 挟まっていた。この入れ替えが起きるのは親指Shiftを押してからの1打鍵目だけで、
 * そこだけShiftが効かない機種があった (issue #18)。
 *
 * あわせて del_mods()/set_mods() をやめる。この2つは real_mods を書き換える
 * だけでレポートを送らないため、親指Shiftを押し続けていてもホスト側では
 * 文字ごとにShiftが離れた状態になっていた。
 *
 * 一時は親指Shiftを左右とも左Shiftに揃えて、weak Shift と同じビットにしていた
 * (issue #37)。今は「み」が右Shift (issue #85) なので、weak Shift を付け直す形に
 * 戻すとこの罠がそのまま復活する。押されている実Shiftを使うのはそのため。 */
static bool process_shift_pair(uint16_t plain, uint16_t shifted, keyrecord_t *record) {
    if (!record->event.pressed) return false;

    const uint8_t shift = get_mods() & MOD_MASK_SHIFT;
    if (!shift) {
        tap_code16(plain);
    } else if (is_shifted_keycode(shifted)) {
        tap_code(QK_MODS_GET_BASIC_KEYCODE(shifted)); // 押されているShiftをそのまま使う
    } else {
        /* shifted 側はShift無しで送る必要がある (「な」→「ほ」など)。
         *
         * 実Shiftを外す前後にウェイトを挟む。挟まないと、親指Shiftのホールドが
         * 確定した直後の1打鍵目だけShiftが乗ったまま解釈されていた (issue #36)。
         * 「な」なら「ほ」ではなく S(KC_MINS) の「ー」が出る。
         *
         * レポート列自体は1打鍵目も2打鍵目も同じで、違うのは間隔だけだった。
         * 別キー割り込みでホールドが確定するため、1打鍵目は
         *   [LSFT] (ホールド確定) → [] (Shiftを外す) → [KC_MINS]
         * が全て同じスキャンで出る。2打鍵目以降はホールド確定から間が空くので
         * 正しく出ていた。つまりIMEがShiftの上げ下げを追い切れていない。 */
        wait_ms(IME_WAIT_MS);
        unregister_mods(shift);
        wait_ms(IME_WAIT_MS);
        tap_code16(shifted);
        register_mods(shift);
    }
    return false;
}

/*
 * 記号とかなのキーの出力表
 */

/* 記号とかなのキー (KN_* / SY_*) は独自キーコードで、ホストへ送るキーはここの表で
 * 引く (issue #73)。同じ文字でも、送るべきキーコードは接続先のキーボード配列で
 * 変わる (「@」は US なら Shift+2、JIS なら単独のキー)。キーマップに素のキーコードを
 * 直接置くとそこを吸収できないので、キーマップは「どの文字のキーか」だけを持ち、
 * 配列ごとの違いは表の列として並べる。
 *
 * 今ある列は US だけで、出力は表にする前と変わらない。JIS の列は後から足す。 */
enum {
    KEY_OUTPUT_COL_US, // English (US)
    KEY_OUTPUT_COL_SIZE,
};

/* 配列の設定 (WINDMILL_LAYOUT_*) から表の列を引く。設定の値をそのまま添字に
 * しないのは、設定のほうはEEPROMに残る値で、既定の JIS を 0 に固定してあるため。
 *
 * JIS の列がまだ無いので、どちらを選んでも US の列を返す (issue #74)。
 * 列を足したら、ここで JIS をそちらへ向ける */
static uint8_t key_output_column(uint8_t layout) {
    switch (layout) {
        case WINDMILL_LAYOUT_JIS:
        case WINDMILL_LAYOUT_US:
        default:
            return KEY_OUTPUT_COL_US;
    }
}

typedef struct {
    uint16_t plain;   // Shift無しで送るキー
    uint16_t shifted; // Shift時に送るキー。KC_NO なら出し分けない (process_key_output 参照)
} key_output_t;

#define AS_IS(keycode) {(keycode), KC_NO}             // Shiftで出し分けない。素のキーと同じに振る舞う
#define PAIR(plain, shifted) {(plain), (shifted)}     // Shift時に別のキーを出す (process_shift_pair 参照)
#define ROW(keycode) ((keycode) - KEY_OUTPUT_FIRST)

/* 行は独自キーコード、列は接続先の配列 (KEY_OUTPUT_COL_*)。
 *
 * かなはOSのIMEがキーの位置で決めるので、US の列は「JISかな配列でそのかなが
 * 載っている位置の、US配列でのキーコード」になる。Shift時のかなのうち、IMEが
 * 同じキーのShiftで出してくれるもの (ぁ、を など) は AS_IS のままでよく、別の
 * キーへ移したものだけ PAIR にしてある。 */
static const key_output_t PROGMEM key_outputs[][KEY_OUTPUT_COL_SIZE] = {
    //                        US
    [ROW(KN_NU)]           = {AS_IS(KC_1)},
    [ROW(KN_FU)]           = {AS_IS(KC_2)},
    [ROW(KN_A)]            = {AS_IS(KC_3)},
    [ROW(KN_U)]            = {AS_IS(KC_4)},
    [ROW(KN_E)]            = {AS_IS(KC_5)},
    [ROW(KN_O)]            = {AS_IS(KC_6)},
    [ROW(KN_YA)]           = {AS_IS(KC_7)},
    [ROW(KN_YU)]           = {AS_IS(KC_8)},
    [ROW(KN_YO)]           = {AS_IS(KC_9)},
    [ROW(KN_WA)]           = {AS_IS(KC_0)},
    [ROW(KN_TA)]           = {AS_IS(KC_Q)},
    [ROW(KN_TE)]           = {PAIR(KC_W, S(KC_EQL))},     // へ
    [ROW(KN_I)]            = {AS_IS(KC_E)},
    [ROW(KN_SU)]           = {PAIR(KC_R, KC_BSLS)},       // む
    [ROW(KN_KA)]           = {AS_IS(KC_T)},
    [ROW(KN_N)]            = {AS_IS(KC_Y)},
    [ROW(KN_NA)]           = {PAIR(KC_U, KC_MINS)},       // ほ
    [ROW(KN_NI)]           = {AS_IS(KC_I)},
    [ROW(KN_RA)]           = {PAIR(KC_O, S(KC_LBRC))},    // 「 (Android は os_key_outputs[])
    [ROW(KN_SE)]           = {PAIR(KC_P, S(KC_RBRC))},    // 」 (同上)
    [ROW(KN_DAKU)]         = {PAIR(KC_LBRC, KC_RBRC)},    // ゜
    [ROW(KN_CHI)]          = {PAIR(KC_A, S(KC_Z))},       // っ
    [ROW(KN_TO)]           = {AS_IS(KC_S)},
    [ROW(KN_SHI)]          = {AS_IS(KC_D)},
    [ROW(KN_HA)]           = {AS_IS(KC_F)},
    [ROW(KN_KI)]           = {AS_IS(KC_G)},
    [ROW(KN_KU)]           = {AS_IS(KC_H)},
    [ROW(KN_MA)]           = {AS_IS(KC_J)},
    [ROW(KN_NO)]           = {PAIR(KC_K, S(KC_COMM))},    // 、
    [ROW(KN_RI)]           = {PAIR(KC_L, S(KC_DOT))},     // 。
    [ROW(KN_RE)]           = {PAIR(KC_SCLN, S(KC_SLSH))}, // ・
    [ROW(KN_KE)]           = {PAIR(KC_QUOT, S(KC_MINS))}, // ー
    [ROW(KN_RU)]           = {AS_IS(KC_DOT)},
    [ROW(KN_ME)]           = {AS_IS(KC_SLSH)},
    [ROW(KN_RO)]           = {AS_IS(KC_GRV)},

    [ROW(SY_SCLN_COLN)]    = {AS_IS(KC_SCLN)},
    [ROW(SY_QUOT_DQUO)]    = {AS_IS(KC_QUOT)},
    [ROW(SY_COMM_LABK)]    = {AS_IS(KC_COMM)},
    [ROW(SY_DOT_RABK)]     = {AS_IS(KC_DOT)},

    [ROW(SY_EXLM)]         = {AS_IS(S(KC_1))},
    [ROW(SY_AT)]           = {AS_IS(S(KC_2))},
    [ROW(SY_HASH)]         = {AS_IS(S(KC_3))},
    [ROW(SY_DLR)]          = {AS_IS(S(KC_4))},
    [ROW(SY_PERC)]         = {AS_IS(S(KC_5))},
    [ROW(SY_CIRC)]         = {AS_IS(S(KC_6))},
    [ROW(SY_AMPR)]         = {AS_IS(S(KC_7))},
    [ROW(SY_ASTR)]         = {AS_IS(S(KC_8))},
    [ROW(SY_LPRN)]         = {AS_IS(S(KC_9))},
    [ROW(SY_RPRN)]         = {AS_IS(S(KC_0))},
    [ROW(SY_GRV)]          = {AS_IS(KC_GRV)},
    [ROW(SY_EQL)]          = {AS_IS(KC_EQL)},
    [ROW(SY_PLUS)]         = {AS_IS(S(KC_EQL))},
    [ROW(SY_MINS)]         = {AS_IS(KC_MINS)},
    [ROW(SY_UNDS)]         = {AS_IS(S(KC_MINS))},
    [ROW(SY_LBRC)]         = {AS_IS(KC_LBRC)},
    [ROW(SY_RBRC)]         = {AS_IS(KC_RBRC)},
    [ROW(SY_TILD)]         = {AS_IS(S(KC_GRV))},
    [ROW(SY_LCBR)]         = {AS_IS(S(KC_LBRC))},
    [ROW(SY_RCBR)]         = {AS_IS(S(KC_RBRC))},
    [ROW(SY_PIPE)]         = {AS_IS(S(KC_BSLS))},
    [ROW(SY_QUES)]         = {AS_IS(S(KC_SLSH))},
};
_Static_assert(ARRAY_SIZE(key_outputs) == ROW(KEY_OUTPUT_LAST) + 1, "key_outputs[] の行が独自キーコードの数と合っていない");
_Static_assert(WINDMILL_KEYCODE_END - 1 <= QK_USER_MAX, "独自キーコードが QK_USER の範囲に収まっていない");

/* 対象OS (MY_WIN / MY_ANDR) で出力が変わるキー。ここに行があれば key_outputs[] より
 * 優先する。列は同じく接続先の配列。
 *
 * 「」はIMEによって載っている位置が違う。Windows (Microsoft IME) は [ と ] の
 * Shiftで出るが、Android (Gboard) は ] と \ のShiftで出る。 */
static const struct {
    uint16_t     keycode;
    uint8_t      os;
    key_output_t outputs[KEY_OUTPUT_COL_SIZE];
} PROGMEM os_key_outputs[] = {
    //                 US
    {KN_RA, OS_ANDR, {PAIR(KC_O, S(KC_RBRC))}}, // 「
    {KN_SE, OS_ANDR, {PAIR(KC_P, S(KC_BSLS))}}, // 」
};

#undef AS_IS
#undef PAIR
#undef ROW

// keycode は KEY_OUTPUT_FIRST 〜 KEY_OUTPUT_LAST であること
static key_output_t get_key_output(uint16_t keycode) {
    const uint8_t column = key_output_column(windmill_host_layout());
    const uint8_t os     = get_host_os();
    key_output_t  output;

    for (uint8_t i = 0; i < ARRAY_SIZE(os_key_outputs); ++i) {
        if (pgm_read_word(&os_key_outputs[i].keycode) == keycode && pgm_read_byte(&os_key_outputs[i].os) == os) {
            memcpy_P(&output, &os_key_outputs[i].outputs[column], sizeof(output));
            return output;
        }
    }
    memcpy_P(&output, &key_outputs[keycode - KEY_OUTPUT_FIRST][column], sizeof(output));
    return output;
}

static bool is_key_output_target(uint16_t keycode) {
    return keycode >= KEY_OUTPUT_FIRST && keycode <= KEY_OUTPUT_LAST;
}

static uint8_t bottom_tap_keycode(uint16_t keycode); // かなレイヤー最下段のタップ・ホールド

uint16_t windmill_output_keycode(uint16_t keycode) {
    if (is_key_output_target(keycode)) return get_key_output(keycode).plain;
    if (keycode >= KEY_BOTTOM_FIRST && keycode <= KEY_BOTTOM_LAST) return bottom_tap_keycode(keycode);
    return keycode;
}

/* 出力表で引くキーを送る。常に false (QMKには渡さない)。
 *
 * 出し分けないキーは、QMKが素のキーコードを処理するのと同じレポート列にする。
 * 以前はキーマップに KC_1 や S(KC_1) を直接置いてQMKに任せていたので、そこから
 * 挙動を変えないため。
 *
 * - 押下で register、解放で unregister。process_shift_pair() のように押下の中で
 *   送り切ると、押しっぱなしにしてもキーリピートが効かなくなる
 * - 押下のたびに weak mods を落とす。QMKは process_action() の頭でこれをやって
 *   いる。落とさないと、「!」(Shift+1) を離す前に「=」を押したとき、「!」の
 *   weak Shift が「=」にも乗って「+」に化ける
 * - 機種側の process_record にも、素のキーコードとして渡す。geonix41 のブロブは
 *   キーマップに直接置いていたころからこれらのキーを見ている */
static bool process_key_output(uint16_t keycode, keyrecord_t *record) {
    const key_output_t output = get_key_output(keycode);
    if (output.shifted != KC_NO) {
        return process_shift_pair(output.plain, output.shifted, record);
    }

    if (!windmill_board_process_record(output.plain, record)) return false;

    if (record->event.pressed) {
        clear_weak_mods();
        register_code16(output.plain);
    } else {
        unregister_code16(output.plain);
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
 * 左右とも左Shiftなので、QMKの修飾ビットは両者で共有される。参照カウントが
 * 無いため、素のままだと先に離したほうの unregister_mods() でShiftが落ち、
 * もう片方を押していてもホストからはShiftが離れて見える。押しっぱなしのほうが
 * 残っているうちは離すイベントごと消費して、持ち替え (ハンドオーバー) を
 * 途切れさせない。Shift+タップ (半角スペース) はそのままQMKに任せる。 */
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
 * かなレイヤー上での記号入力
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

/* 記号レイヤーの数字・記号キーか (レイヤー配置と一致させること)。
 * 矢印キーとKC_TRNSで下位レイヤーに落ちるキーは含めない。
 *
 * 記号は記号レイヤー専用の独自キーコードなので、かなレイヤーの同じキーコードを
 * 巻き込む心配が無い。数字だけは素のキーコードのまま (windmill.h 参照) */
static bool is_sym_ime_wrap_target(uint16_t keycode) {
    switch (keycode) {
        case KC_1 ... KC_0:
        case SY_EXLM ... SY_QUES:
            return true;
    }
    return false;
}

/* かなレイヤーの「も」(KB_MO) をShiftを押しながらタップしたら、半角「?」を送る。
 * かな入力のままでは打てない記号なので、Symレイヤーの数字・記号と同じ要領で英数へ
 * 切り替えてから送出し、かなへ戻す (issue #17)。
 *
 * Symレイヤー側の is_sym_ime_wrap_target と違い、こちらは押しっぱなしの実Shift
 * (親指Shiftを含む) がある状態で走るので、Shiftの扱いに2つ気をつける点がある。
 *
 * 1. KC_LNG2/KC_LNG1 にShiftが乗っているとIMEが英数/かなキーとして受け取らず、
 *    モードがかなのまま KC_SLSH だけが届いて「・」(JISかなのShift+/) が出る。
 *    そこでモード切り替えの間だけShiftを外す。
 * 2. 「?」のShiftは、外した実Shiftをそのまま戻して使う。以前は S(KC_SLSH) の
 *    weak Shift (左Shift) を自前で付けていたが、それだとホストからは
 *    「右Shiftを離す → 左Shiftを押す」と修飾が入れ替わって見え、言語切り替え
 *    直後の1打鍵目だけShiftが効かずに半角「/」が出ていた。issue #18 と同じ罠。
 *    「み」は右Shift (issue #85) なので、実Shiftを使わないとこの罠を踏む。
 *
 * 加えて、モードとShiftの変化はどちらもIMEが非同期に処理するので、レポートを
 * 送るたびにウェイトを挟んで順序を保証する。修飾の上げ下げにも1回ずつ要るのは、
 * Shiftを押した直後に KC_SLSH を送るとIMEが切り替え処理に紛れてShiftを取り
 * こぼすため。del_mods()/set_mods() ではなく register/unregister を使うのは、
 * ホスト側にもShiftの上げ下げを届ける必要があるから (これも issue #18)。
 *
 * shift は押されている実Shift (MOD_BIT)。タップの判定は send_bottom_tap() が持つ */
static void send_kana_qmark(uint8_t shift) {
    unregister_mods(shift); // モード切り替えの間だけ実Shiftを外す
    wait_ms(IME_WAIT_MS);
    tap_code16(KC_LNG2); // 英数へ
    wait_ms(IME_WAIT_MS);
    register_mods(shift); // 押されていたShiftをそのまま戻して「?」に使う
    wait_ms(IME_WAIT_MS);
    tap_code(KC_SLSH);
    wait_ms(IME_WAIT_MS);
    unregister_mods(shift);
    wait_ms(IME_WAIT_MS);
    tap_code16(KC_LNG1); // かなへ
    wait_ms(IME_WAIT_MS);
    register_mods(shift); // 押しっぱなしの実Shiftを戻す
}

/*
 * 英数レイヤーの Win / Alt と Ctrl
 */

/* Ctrlを先に押していると英数レイヤーが既に上がっていて、つ / さ の位置は
 * 英数レイヤー側の素の KC_LGUI / KC_LALT として解決される。そちらも数えないと、
 * Ctrlを先に離した時点でレイヤーが落ちてしまう (issue #34)。
 *
 * 解放は押下と同じ英数レイヤーのキーコードで届く (QMKが押下時のレイヤーを
 * 覚えている) ので、押下と解放の対で上げ下げすればよい。英数ベースのときは
 * 英数レイヤーが元から有効なので何もしない */
static void process_alpha_gui_alt(uint16_t keycode, keyrecord_t *record) {
    const uint8_t mod = (keycode == KC_LGUI) ? MOD_BIT(KC_LEFT_GUI) : MOD_BIT(KC_LEFT_ALT);
    if (!record->event.pressed) {
        hold_layer_off(mod);
        return;
    }
    if (get_highest_layer(default_layer_state) != LAYER_KANA) return;
    hold_layer_on(mod);
}

/* Ctrl (MY_LCTL) で上げたレイヤーを下ろすのは、キーの解放イベントではなく
 * **ホストへ出ている修飾**を毎スキャン見て判断する。
 *
 * MY_LCTL の位置は英数レイヤーで透過なので、今のところ解放は MY_LCTL のまま
 * 届く。ただ、自分で英数レイヤーを上げたせいで解放が別のキーコードに引き直される
 * と、MY_LCTL の解放を待っていても永久に来ない (以前の つ / さ の mod-tap がこの罠を
 * 踏んでいた)。修飾の状態を見るなら英数レイヤーの並びを変えても壊れない。
 * ただしレイヤーを上げた直後は、QMKがまだ修飾を register していないスキャンが
 * ありうる。一度ホストへ出たのを確認してから下ろす。 */
static void update_hold_layer(void) {
    const uint8_t ctrl = MOD_BIT(KC_LEFT_CTRL);
    if (!(hold_layer_mods & ctrl)) return;

    if (get_mods() & MOD_MASK_CTRL) {
        hold_layer_ctrl_seen = true;
    } else if (hold_layer_ctrl_seen) {
        hold_layer_off(ctrl);
    }
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
 * かなレイヤーの「つ」「さ」は隣り合った Win / Alt なので、続けて打つだけで
 * 前者を踏む。両方が未判定のまま揃ってホールドになることもあるし、「つ」の
 * ホールドで英数レイヤーが上がった後なら「さ」は素の KC_LALT になる。
 * 前者は apply_bottom_hold()、後者はここで、後から出るほうを握りつぶす。 */

// 後から出そうとした修飾を握りつぶすか。押下時のOS設定で決める
static bool android_blocks_mod(uint8_t mod) {
    const uint8_t other = (mod & MOD_MASK_GUI) ? MOD_MASK_ALT : MOD_MASK_GUI;
    return is_android() && (get_mods() & other);
}

/* 押下を握りつぶした修飾 (MOD_BIT)。離すイベントも消費するために覚える。
 * 握りつぶしたほうは、先に押したほうを離しても出さない。出すと押した覚えの無い
 * 修飾が遅れて効くことになる */
static uint8_t android_blocked_mods = 0;

/* 英数レイヤーの KC_LGUI / KC_LALT。後から押したほうをホストへ出さない。
 * falseを返したらそのイベントは消費済み(以降の処理をスキップ) */
static bool process_android_gui_alt(uint16_t keycode, keyrecord_t *record) {
    const uint8_t mod = (keycode == KC_LGUI) ? MOD_BIT(KC_LEFT_GUI) : MOD_BIT(KC_LEFT_ALT);

    if (!record->event.pressed) {
        // 押下時のOS設定は見ない。押している間に設定を変えても離すぶんは消費する
        if (!(android_blocked_mods & mod)) return true;
        android_blocked_mods &= ~mod;
        return false;
    }
    if (!android_blocks_mod(mod)) return true;

    android_blocked_mods |= mod;
    return false;
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
 * 押下の中で送り切って、Win を外している間にスキャンを挟まない。
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
 * 設定レイヤー
 */

/* 左右のFnを両方ホールドしている間だけ設定レイヤーへ移す (issue #62)。
 *
 * Fnレイヤーの Fn の位置へ MO(4) を置けばコードは要らないが、それだと設定レイヤーを
 * 上げているのは後から押したほうのFnだけになる。先に押したほうを離しても
 * 設定レイヤーが残り、「両方押している間だけ」にならない。そこでホールド中の
 * Fnを左右1ビットずつ覚えて、Fnレイヤーと設定レイヤーの上げ下げを両方とも
 * ここで持つ。QMKのレイヤーは参照カウントを持たないので、Fnレイヤーのほうも
 * QMK の MO() に任せると、先に離したほうがまだ押している側のぶんまで落としてしまう。
 *
 * 呼ぶのは、かなレイヤーの そ / ね のホールド (apply_bottom_hold) と、英数レイヤーの
 * MO(3) (process_fn)。どちらも同じビットを使うので、混ぜて押しても数が合う。
 *
 * 設定レイヤーの Fn の位置は透過にしておくこと。片方を離して押し直したとき、
 * その押下は設定レイヤーで引かれる。透過でないと下のレイヤーの Fn に落ちず、
 * Fn として押せなくなる */
static uint8_t fn_held = 0;

#define FN_BIT_L 1
#define FN_BIT_R 2
#define FN_BOTH (FN_BIT_L | FN_BIT_R)

static void set_fn(uint8_t bit, bool on) {
    if (on) {
        fn_held |= bit;
    } else {
        fn_held &= ~bit;
    }

    layer_state_t state = layer_state & ~(((layer_state_t)1 << LAYER_FN) | ((layer_state_t)1 << LAYER_CONF));
    if (fn_held) state |= (layer_state_t)1 << LAYER_FN;
    if (fn_held == FN_BOTH) state |= (layer_state_t)1 << LAYER_CONF;
    layer_state_set(state);
}

/* 英数レイヤーの MO(3)。左右は matrix の列で見分ける (windmill.h 参照)。
 * レイヤーの上げ下げは全て set_fn() が持つので、QMKには渡さない */
static bool process_fn(keyrecord_t *record) {
    set_fn(record->event.key.col == FN_L_COL ? FN_BIT_L : FN_BIT_R, record->event.pressed);
    return false;
}

/* 記号レイヤー。かなレイヤーの ひ / も のどちらかをホールドしている間だけ上げる。
 * Fnと同じく左右1ビットずつ覚えて、片方を離しても落ちないようにする */
static uint8_t sym_held = 0;

static void set_sym(uint8_t bit, bool on) {
    if (on) {
        sym_held |= bit;
        layer_on(LAYER_SYM);
    } else {
        sym_held &= ~bit;
        if (!sym_held) layer_off(LAYER_SYM);
    }
}

/*
 * かなレイヤー最下段のタップ・ホールド
 */

/* かなレイヤー最下段の8キー (KB_*) は、タップでかなを出し、ホールドで修飾か
 * レイヤーになる。以前は QMK の LT() / MT() だったが、判定を自前で持つ (issue #85)。
 * QMK の tap-hold はキーごとに独立して決まるので、親指Shift同士の組で半角スペースを
 * 出す規則や、未判定のキーを複数抱えたまま押した順に出す規則を表せない。
 *
 * - ホールドに決まるのは、押してから BOTTOM_HOLD_MS たったとき (単独の長押しを含む)、
 *   または BOTTOM_INTERRUPT_MS 以上たっていて、後から最下段以外のキーが押されているとき
 * - 未判定のまま離せばタップ
 * - 最下段のキーの押下では、先に押された未判定のキーを判定しない。後から押した
 *   最下段のキーがタップに決まった時点で、先のキーが BOTTOM_INTERRUPT_MS 以上たって
 *   いれば、先のキーをホールドにする (decide_bottom_tap)
 * - こ / み の組は別の規則で半角スペースになる (settle_thumb_pair)
 *
 * 未判定のキーより後に起きたキーイベントは、そのキーが決まるまでキューで待たせる。
 * 押した順を保つためで、最下段のキーのタップも同じキューに並ぶ。待っていたキーは
 * 取り出した時点のレイヤーと修飾で解決し直す。ホールドの規則から、BOTTOM_INTERRUPT_MS
 * 以上たったキーは別のキーの押下で決まるので、待たされるのはそれ未満の間に
 * 押したキーだけになる。
 *
 * 時間の判定はスキャンごと (matrix_scan_kb) に行い、キーイベントの前にもその時刻で
 * 行う。境界はどちらも「以上」。250ms ちょうどに離したキーはホールドになる。
 *
 * タップは押下と解放を続けて送るので、押しっぱなしにしてもリピートしない。 */
#define BOTTOM_INTERRUPT_MS 150 // これ以上たっていれば、別のキーの押下でホールドになる
#define BOTTOM_HOLD_MS      250 // 押したままこれだけたてばホールドになる

#define BOTTOM_COUNT (KEY_BOTTOM_LAST - KEY_BOTTOM_FIRST + 1)
#define BOTTOM(keycode) ((keycode) - KEY_BOTTOM_FIRST)
#define NO_BOTTOM 0xFF

static bool is_bottom_keycode(uint16_t keycode) {
    return keycode >= KEY_BOTTOM_FIRST && keycode <= KEY_BOTTOM_LAST;
}

// ホールドの役割
enum {
    BOTTOM_HOLD_OS_MOD, // Win / Alt。ホールド中は英数レイヤーへ移す (issue #34)
    BOTTOM_HOLD_FN,     // Fn。左右両方で設定レイヤー (set_fn)
    BOTTOM_HOLD_SYM,    // 記号レイヤー (set_sym)
    BOTTOM_HOLD_SHIFT,  // Shift。左右それぞれ自分の Shift だけを上げ下げする
};

typedef struct {
    uint8_t tap;     // タップで送るキー。かなはOSのIMEが位置で決める
    uint8_t hold;    // BOTTOM_HOLD_*
    uint8_t arg;     // 修飾なら MOD_BIT、Fn / 記号なら左右のビット
    uint8_t partner; // 同じ役割の相方。NO_BOTTOM なら無し
} bottom_key_t;

static const bottom_key_t PROGMEM bottom_keys[] = {
    [BOTTOM(KB_TSU)] = {KC_Z, BOTTOM_HOLD_OS_MOD, MOD_BIT(KC_LEFT_GUI), NO_BOTTOM},
    [BOTTOM(KB_SA)]  = {KC_X, BOTTOM_HOLD_OS_MOD, MOD_BIT(KC_LEFT_ALT), NO_BOTTOM},
    [BOTTOM(KB_SO)]  = {KC_C, BOTTOM_HOLD_FN, FN_BIT_L, BOTTOM(KB_NE)},
    [BOTTOM(KB_HI)]  = {KC_V, BOTTOM_HOLD_SYM, 1, BOTTOM(KB_MO)},
    [BOTTOM(KB_KO)]  = {KC_B, BOTTOM_HOLD_SHIFT, MOD_BIT(KC_LEFT_SHIFT), BOTTOM(KB_MI)},
    [BOTTOM(KB_MI)]  = {KC_N, BOTTOM_HOLD_SHIFT, MOD_BIT(KC_RIGHT_SHIFT), BOTTOM(KB_KO)},
    [BOTTOM(KB_MO)]  = {KC_M, BOTTOM_HOLD_SYM, 2, BOTTOM(KB_HI)},
    [BOTTOM(KB_NE)]  = {KC_COMMA, BOTTOM_HOLD_FN, FN_BIT_R, BOTTOM(KB_SO)},
};
_Static_assert(ARRAY_SIZE(bottom_keys) == BOTTOM_COUNT, "bottom_keys[] の行が最下段のキーの数と合っていない");

static bottom_key_t get_bottom_key(uint8_t i) {
    bottom_key_t key;
    memcpy_P(&key, &bottom_keys[i], sizeof(key));
    return key;
}

static uint8_t bottom_tap_keycode(uint16_t keycode) {
    return get_bottom_key(BOTTOM(keycode)).tap;
}

static bool is_thumb_key(uint8_t i) {
    return i == BOTTOM(KB_KO) || i == BOTTOM(KB_MI);
}

// 判定の結果
enum {
    BOTTOM_UNDECIDED,
    BOTTOM_TAP,
    BOTTOM_HOLD,
    BOTTOM_SPACE,   // こ / み の組の半角スペース
    BOTTOM_NOTHING, // 何も出さない (組のスペースを相方が出す)
};

/* キーごとの、いちばん新しい押下の状態 */
typedef struct {
    keypos_t pos;
    uint16_t pressed_at;
    uint8_t  seq;         // 押下の通し番号。押した順の比較と、キューの項目との対応に使う
    uint8_t  decision;    // BOTTOM_UNDECIDED など
    bool     pressed;     // 物理的に押されている
    bool     interrupted; // 押してから、最下段以外のキーが押された
    bool     used;        // ホールドとして他のキーにかかった
} bottom_state_t;

static bottom_state_t bottom_states[BOTTOM_COUNT];
static uint8_t        bottom_seq   = 0;
static uint8_t        bottom_held  = 0;         // ホールドを出しているキー (1 << 番号)
static uint8_t        thumb_pair_y = NO_BOTTOM; // 親指の組の、後から押したほう (settle_thumb_pair)
static bool           thumb_pair_ready = false; // Y を離した時点で、X も Y もシフトとしてかかっていなかった

/* 出力を待たせているイベント。押した順に並ぶ */
enum {
    BOTTOM_ENTRY_PRESS,   // 最下段のキーの押下。判定が決まるまで先頭で止まる
    BOTTOM_ENTRY_RELEASE, // 最下段のキーの解放。ホールドを下ろす
    BOTTOM_ENTRY_RECORD,  // それ以外のキーのイベント
};

typedef struct {
    uint8_t     type;
    uint8_t     index;    // 最下段のキーの番号 (BOTTOM_ENTRY_PRESS / RELEASE)
    uint8_t     seq;      // BOTTOM_ENTRY_PRESS: 押下の通し番号
    uint8_t     decision; // BOTTOM_ENTRY_PRESS: 同じキーが押し直された後の、この押下の結果
    keyrecord_t record;
} bottom_entry_t;

/* 待たせられるのは、未判定のキーを押してから BOTTOM_HOLD_MS の間に起きたイベント
 * だけ。両手で打っても収まる数にしてある。溢れたら先頭を決めて空ける */
#define BOTTOM_QUEUE_SIZE 16

static bottom_entry_t bottom_queue[BOTTOM_QUEUE_SIZE];
static uint8_t        bottom_queue_len = 0;
static uint8_t        bottom_sent      = 0; // キューから出した数。判定が動いたかを見る
static bool           bottom_replaying = false;

static int16_t bottom_age(const bottom_state_t *state, uint16_t now) {
    return (int16_t)(now - state->pressed_at);
}

static bottom_entry_t *find_bottom_press_entry(uint8_t i) {
    for (uint8_t n = 0; n < bottom_queue_len; ++n) {
        bottom_entry_t *entry = &bottom_queue[n];
        if (entry->type == BOTTOM_ENTRY_PRESS && entry->index == i && entry->seq == bottom_states[i].seq) return entry;
    }
    return NULL;
}

static uint8_t find_pressed_bottom(keypos_t pos) {
    for (uint8_t i = 0; i < BOTTOM_COUNT; ++i) {
        const bottom_state_t *state = &bottom_states[i];
        if (state->pressed && state->pos.row == pos.row && state->pos.col == pos.col) return i;
    }
    return NO_BOTTOM;
}

// 押し直されていなければ今の状態、押し直されていれば項目へ移した結果
static uint8_t bottom_entry_decision(const bottom_entry_t *entry) {
    const bottom_state_t *state = &bottom_states[entry->index];
    return state->seq == entry->seq ? state->decision : entry->decision;
}

/* イベントを待たせたまま、もう一度 QMK に処理させる。キーコードはその時点の
 * レイヤーで引き直される */
static void bottom_replay(keyrecord_t *record) {
    const bool replaying = bottom_replaying;
    bottom_replaying     = true;
    process_record(record);
    bottom_replaying = replaying;
}

/* 未判定のキーをタップに決める。
 *
 * タップに決まった時点で、先に押された未判定のキーが BOTTOM_INTERRUPT_MS 以上
 * たっていれば、そちらをホールドにする。後のキーはそのホールドのレイヤーで出る
 * (ひ を押しながら こ をタップすると、記号レイヤーの「|」) */
static void decide_bottom_tap(uint8_t i, uint16_t now) {
    bottom_state_t *state = &bottom_states[i];
    state->decision       = BOTTOM_TAP;

    for (uint8_t k = 0; k < BOTTOM_COUNT; ++k) {
        bottom_state_t *earlier = &bottom_states[k];
        if (k == i || !earlier->pressed || earlier->decision != BOTTOM_UNDECIDED) continue;
        if ((int8_t)(earlier->seq - state->seq) >= 0) continue; // 後から押されたもの
        if (bottom_age(earlier, now) < BOTTOM_INTERRUPT_MS) continue;
        earlier->decision = BOTTOM_HOLD;
        earlier->used     = true;
    }
}

/*
 * 親指の組 (こ / み)
 *
 * 先に押したほうを X、X を押している間に押した相方を Y とする。Y のタップごとに、
 * 次をすべて満たすと半角スペースを1つ出す。
 *
 * - X を BOTTOM_INTERRUPT_MS 以上押している (離したなら、押下から解放までが以上)
 * - Y の押下が X の解放より前 (組ができる条件そのもの)
 * - Y の押下から解放までが BOTTOM_HOLD_MS 未満 (Y が未判定のまま離れる)
 * - Y の解放の時点で、X と Y のどちらも他のキーにシフトとしてかかっていない
 *
 * スペースは、Y の解放と、X が BOTTOM_INTERRUPT_MS に達した時点 (それより前に X を
 * 離したなら X の解放) の遅い方で決まる。シフトとしてかかったかどうかは Y の解放の
 * 時点で見るので、決まったスペースは後から X がかかっても取り消さない。X を押したまま
 * Y をタップし直せば、そのたびに新しい組になってスペースが出る。
 *
 * スペースが決まったとき X をまだ押していれば、X はホールドにする。X のタップは
 * スペースに含まれたので出さず、続けて打つキーには Shift として乗る。X を
 * BOTTOM_INTERRUPT_MS 以上押して先に離していれば、X の出力も Y の結果を待つ。
 *
 * 満たさなくなった時点で組は解け、それぞれ通常の判定に戻る (thumb_pair_fail)。
 * X を短く離せば X はタップ、Y が BOTTOM_HOLD_MS に達すれば Y はホールドで、
 * 待っていた X はタップになる。
 */

static bool in_thumb_pair(uint8_t i) {
    return thumb_pair_y != NO_BOTTOM && (i == thumb_pair_y || i == get_bottom_key(thumb_pair_y).partner);
}

// 組を解いて、離れたまま待っていたほうを通常の判定に戻す
static void thumb_pair_fail(uint16_t now) {
    const uint8_t y = thumb_pair_y;
    const uint8_t x = get_bottom_key(y).partner;
    thumb_pair_y    = NO_BOTTOM;

    if (bottom_states[x].decision == BOTTOM_UNDECIDED && !bottom_states[x].pressed) decide_bottom_tap(x, now);
    if (bottom_states[y].decision == BOTTOM_UNDECIDED && !bottom_states[y].pressed) decide_bottom_tap(y, now);
}

static void settle_thumb_pair(uint16_t now) {
    if (thumb_pair_y == NO_BOTTOM) return;

    const uint8_t   y  = thumb_pair_y;
    const uint8_t   x  = get_bottom_key(y).partner;
    bottom_state_t *xs = &bottom_states[x];
    bottom_state_t *ys = &bottom_states[y];

    // Y がホールドになった (他のキーにかかった場合を含む) / X を短く離してタップに決まった
    const bool x_tapped = xs->decision != BOTTOM_UNDECIDED && xs->decision != BOTTOM_HOLD;
    if (ys->decision == BOTTOM_HOLD || x_tapped) {
        thumb_pair_fail(now);
        return;
    }
    if (ys->pressed) return; // Y の解放を待つ
    if (!thumb_pair_ready) {
        thumb_pair_fail(now); // Y の解放の時点で、どちらかがシフトとしてかかっていた
        return;
    }
    if (xs->decision == BOTTOM_UNDECIDED && xs->pressed && bottom_age(xs, now) < BOTTOM_INTERRUPT_MS) {
        return; // X が BOTTOM_INTERRUPT_MS に達するか、離すのを待つ
    }

    // 成立。スペースは出力が先に並んでいるほうで出す
    thumb_pair_y = NO_BOTTOM;
    if (xs->decision == BOTTOM_UNDECIDED && !xs->pressed) {
        xs->decision = BOTTOM_SPACE; // 離したまま Y を待っていた
        ys->decision = BOTTOM_NOTHING;
        return;
    }
    if (xs->decision == BOTTOM_UNDECIDED) xs->decision = BOTTOM_HOLD; // 押したままなら Shift として残す
    ys->decision = BOTTOM_SPACE;
}

/*
 * ホールドとタップの送出
 */

static void apply_bottom_hold(uint8_t i) {
    const bottom_key_t key = get_bottom_key(i);
    switch (key.hold) {
        case BOTTOM_HOLD_OS_MOD:
            // Android では Win と Alt の後から出るほうを握りつぶす (issue #57)
            if (android_blocks_mod(key.arg)) return;
            register_mods(key.arg);
            hold_layer_on(key.arg);
            break;
        case BOTTOM_HOLD_FN:
            set_fn(key.arg, true);
            break;
        case BOTTOM_HOLD_SYM:
            set_sym(key.arg, true);
            break;
        case BOTTOM_HOLD_SHIFT:
            register_mods(key.arg);
            break;
    }
    bottom_held |= 1 << i;
}

static void release_bottom_hold(uint8_t i) {
    if (!(bottom_held & (1 << i))) return; // タップだった、または握りつぶした
    bottom_held &= ~(1 << i);

    const bottom_key_t key = get_bottom_key(i);
    switch (key.hold) {
        case BOTTOM_HOLD_OS_MOD:
            unregister_mods(key.arg);
            hold_layer_off(key.arg);
            break;
        case BOTTOM_HOLD_FN:
            set_fn(key.arg, false);
            break;
        case BOTTOM_HOLD_SYM:
            set_sym(key.arg, false);
            break;
        case BOTTOM_HOLD_SHIFT:
            unregister_mods(key.arg);
            break;
    }
}

/* タップを出す。押されている Shift はそのまま乗る。
 *
 * キーは出す時点のレイヤーで引き直す。先に押したキーのホールドで記号レイヤーが
 * 上がっていれば、そこにあるキーを出す。記号レイヤーの最下段は大半が透過で、
 * そのときは自分のかなが出る。自分以外のキーなら、押下と解放を作って QMK に
 * 処理させる (記号はIMEを英数に切り替えて送るなど、通常のキーと同じ扱いになる) */
static void send_bottom_tap(uint8_t i, keypos_t pos) {
    const uint16_t keycode = keymap_key_to_keycode(layer_switch_get_layer(pos), pos);
    if (keycode != KEY_BOTTOM_FIRST + i) {
        keyrecord_t record = {.event = MAKE_KEYEVENT(pos.row, pos.col, true)};
        bottom_replay(&record);
        record.event = (keyevent_t)MAKE_KEYEVENT(pos.row, pos.col, false);
        bottom_replay(&record);
        return;
    }

    const uint8_t shift = get_mods() & MOD_MASK_SHIFT;
    if (i == BOTTOM(KB_MO) && shift) {
        send_kana_qmark(shift); // も の Shift+タップは半角「?」(issue #17)
        return;
    }
    tap_code(get_bottom_key(i).tap);
}

/* 親指の組の半角スペース。組のキーはどちらも離れているので、ホールドとして出ている
 * Shift はここで下ろす (後から届く解放では何もしない)。
 *
 * Shift無しで送る。Shift を外した直後にスペースを送ると、IMEがShiftの上げ下げを
 * 追い切れず、Shiftが乗ったまま解釈されることがある (issue #36)。del_mods() ではなく
 * unregister 系で外し、ウェイトを挟んで順序を保証する。組の外の Shift (相方では
 * ない別のキーなど) が押されていれば、送った後に戻す */
static void send_thumb_space(void) {
    const bool had_shift = get_mods() & MOD_MASK_SHIFT;
    if (!bottom_states[BOTTOM(KB_KO)].pressed) release_bottom_hold(BOTTOM(KB_KO));
    if (!bottom_states[BOTTOM(KB_MI)].pressed) release_bottom_hold(BOTTOM(KB_MI));

    const uint8_t shift = get_mods() & MOD_MASK_SHIFT;
    if (shift) unregister_mods(shift);
    if (had_shift) wait_ms(IME_WAIT_MS);
    tap_code(KC_SPC);
    if (shift) {
        wait_ms(IME_WAIT_MS);
        register_mods(shift);
    }
}

/* 先頭から、決まっているものを順に出す。未判定の押下に当たったら止まる */
static void bottom_flush(void) {
    while (bottom_queue_len) {
        bottom_entry_t entry    = bottom_queue[0];
        const uint8_t  decision = entry.type == BOTTOM_ENTRY_PRESS ? bottom_entry_decision(&entry) : BOTTOM_UNDECIDED;
        if (entry.type == BOTTOM_ENTRY_PRESS && decision == BOTTOM_UNDECIDED) return;

        --bottom_queue_len;
        memmove(&bottom_queue[0], &bottom_queue[1], bottom_queue_len * sizeof(bottom_queue[0]));
        ++bottom_sent;

        switch (entry.type) {
            case BOTTOM_ENTRY_PRESS:
                switch (decision) {
                    case BOTTOM_TAP:
                        send_bottom_tap(entry.index, entry.record.event.key);
                        break;
                    case BOTTOM_HOLD:
                        apply_bottom_hold(entry.index);
                        break;
                    case BOTTOM_SPACE:
                        send_thumb_space();
                        break;
                }
                break;
            case BOTTOM_ENTRY_RELEASE:
                release_bottom_hold(entry.index);
                break;
            case BOTTOM_ENTRY_RECORD:
                bottom_replay(&entry.record);
                break;
        }
    }
}

/* 未判定のキーを今すぐ決める。キューが溢れそうなときと、同じキーが押し直された
 * ときに使う。押していればホールド、離していればタップ */
static void force_bottom_decision(uint8_t i, uint16_t now) {
    if (in_thumb_pair(i)) thumb_pair_fail(now);

    bottom_state_t *state = &bottom_states[i];
    if (state->decision != BOTTOM_UNDECIDED) return;
    if (state->pressed) {
        state->decision = BOTTOM_HOLD;
        state->used     = true; // 後ろに待っているキーにかかる
    } else {
        decide_bottom_tap(i, now);
    }
}

static void bottom_push(uint8_t type, uint8_t index, const keyrecord_t *record) {
    while (bottom_queue_len >= BOTTOM_QUEUE_SIZE) {
        // 先頭は未判定の押下 (決まっていれば bottom_flush が出している)
        force_bottom_decision(bottom_queue[0].index, record->event.time);
        settle_thumb_pair(record->event.time);
        bottom_flush();
    }

    bottom_entry_t *entry = &bottom_queue[bottom_queue_len++];
    entry->type           = type;
    entry->index          = index;
    entry->seq            = index == NO_BOTTOM ? 0 : bottom_states[index].seq;
    entry->decision       = BOTTOM_UNDECIDED;
    entry->record         = *record;
}

/*
 * イベントごとの判定
 */

static void bottom_tick(uint16_t now) {
    for (uint8_t i = 0; i < BOTTOM_COUNT; ++i) {
        bottom_state_t *state = &bottom_states[i];
        if (!state->pressed || state->decision != BOTTOM_UNDECIDED) continue;

        const int16_t age = bottom_age(state, now);
        if (age >= BOTTOM_HOLD_MS) {
            state->decision = BOTTOM_HOLD;
        } else if (age >= BOTTOM_INTERRUPT_MS && state->interrupted) {
            state->decision = BOTTOM_HOLD;
            state->used     = true; // 割り込んだキーは、このホールドの下で出る
        }
    }
    settle_thumb_pair(now);
    bottom_flush();
}

static void bottom_press(uint8_t i, keyrecord_t *record) {
    const uint16_t  now   = record->event.time;
    bottom_state_t *state = &bottom_states[i];

    // 前の押下がまだキューで待っていれば、その結果を項目へ移してから押し直す
    bottom_entry_t *previous = find_bottom_press_entry(i);
    if (previous) {
        force_bottom_decision(i, now);
        previous->decision = state->decision;
    }

    *state = (bottom_state_t){
        .pos        = record->event.key,
        .pressed_at = now,
        .seq        = ++bottom_seq,
        .decision   = BOTTOM_UNDECIDED,
        .pressed    = true,
    };

    if (is_thumb_key(i) && bottom_states[get_bottom_key(i).partner].pressed) {
        thumb_pair_y     = i;
        thumb_pair_ready = false;
    }

    bottom_push(BOTTOM_ENTRY_PRESS, i, record);
}

static void bottom_release(uint8_t i, keyrecord_t *record) {
    const uint16_t  now   = record->event.time;
    bottom_state_t *state = &bottom_states[i];
    state->pressed        = false;

    if (state->decision == BOTTOM_UNDECIDED) {
        if (!in_thumb_pair(i)) {
            decide_bottom_tap(i, now);
        } else if (i == thumb_pair_y) {
            // シフトとしてかかったかは、Y を離したこの時点で見る。結果は settle_thumb_pair で決まる
            thumb_pair_ready = !bottom_states[get_bottom_key(i).partner].used;
        } else if (bottom_age(state, now) < BOTTOM_INTERRUPT_MS) {
            decide_bottom_tap(i, now); // X を短く離した。組は settle_thumb_pair で解ける
        }
        // それ以外の X は、Y の結果を待つ
    }

    if (bottom_queue_len) {
        bottom_push(BOTTOM_ENTRY_RELEASE, i, record); // 先に押されたキーより後で下ろす
    } else {
        release_bottom_hold(i);
    }
}

// 最下段以外のキーの押下
static void bottom_interrupt(uint16_t now) {
    for (uint8_t i = 0; i < BOTTOM_COUNT; ++i) {
        bottom_state_t *state = &bottom_states[i];
        if (!state->pressed) continue;

        if (state->decision == BOTTOM_HOLD) {
            state->used = true;
        } else if (state->decision == BOTTOM_UNDECIDED) {
            const int16_t age = bottom_age(state, now);
            if (age < 0) continue; // このキーより前の押下 (QMKの tap-hold で遅れて届いた MY_LCTL など)
            if (age >= BOTTOM_INTERRUPT_MS) {
                state->decision = BOTTOM_HOLD;
                state->used     = true;
            } else {
                state->interrupted = true;
            }
        }
    }
}

/* 全てのキーイベントの入口。falseを返したらそのイベントは消費済み
 * (キューで待たせたか、最下段のキーとして処理した) */
static bool process_bottom_row(uint16_t keycode, keyrecord_t *record) {
    const uint16_t now  = record->event.time;
    const uint8_t  sent = bottom_sent;
    bottom_tick(now);

    bool consumed = true;
    if (record->event.pressed) {
        if (is_bottom_keycode(keycode)) {
            bottom_press(BOTTOM(keycode), record);
        } else {
            bottom_interrupt(now);
            consumed = false;
        }
    } else {
        /* 最下段のキーの解放は位置で見分ける。タップを記号レイヤーで出したあとなどは、
         * QMKが覚えている押下時のレイヤーが変わっていて、キーコードが別物で届く */
        const uint8_t i = find_pressed_bottom(record->event.key);
        if (i != NO_BOTTOM) {
            bottom_release(i, record);
        } else if (!is_bottom_keycode(keycode)) {
            consumed = false;
        }
        // 押下を見ていない最下段のキーの解放 (LEDの点灯だけで消費された押下など) は捨てる
    }
    settle_thumb_pair(now);
    bottom_flush();
    if (consumed) return false;

    if (bottom_queue_len) {
        bottom_push(BOTTOM_ENTRY_RECORD, NO_BOTTOM, record);
        return false;
    }
    if (record->event.pressed && bottom_sent != sent) {
        /* このイベントの前にホールドが出て、レイヤーが変わったかもしれない。
         * QMKはもう押下前のレイヤーでキーコードを引いているので、引き直させる */
        bottom_replay(record);
        return false;
    }
    return true;
}

/*
 * QMK callbacks
 */

bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    windmill_board_pre_process_record(keycode, record);
    return pre_process_record_user(keycode, record);
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
#ifdef WINDMILL_LED_ENABLE
    if (!process_led_timeout(keycode, record)) return false;
#endif
    /* かなレイヤー最下段のタップ・ホールド。待たせたイベントは後で取り出して
     * ここへ流し直すので、そのときは通さない */
    if (!bottom_replaying && !process_bottom_row(keycode, record)) return false;
    if (is_bottom_keycode(keycode)) return false; // 流し直しで最下段のキーに引かれた。押下を見ていないので捨てる

    if (!process_record_user(keycode, record)) return false;

    /* ベースがかなのとき、記号レイヤーの数字・記号は
     * 英数(LNG2)に切り替えて送出し、かな(LNG1)に戻す */
    if (layer_state_is(LAYER_SYM) && get_highest_layer(default_layer_state) == LAYER_KANA && is_sym_ime_wrap_target(keycode)) {
        if (record->event.pressed) {
            // IMEのモード切り替えは非同期なので、間にウェイトを挟まないと
            // 切り替え完了前にキーが届いて全角のまま入力される
            tap_code16(KC_LNG2);
            wait_ms(IME_WAIT_MS);
            tap_code16(windmill_output_keycode(keycode));
            wait_ms(IME_WAIT_MS);
            tap_code16(KC_LNG1);
        }
        return false; // releaseも消費 (未registerのunregisterを防ぐ)
    }

    if (is_key_output_target(keycode)) { // 記号とかなのキー
        // Android では Win+. を Alt+. に読み替える (issue #57)
        if (keycode == SY_DOT_RABK && !process_android_gui_dot(record)) {
            return false;
        }
        return process_key_output(keycode, record);
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

        case KC_LGUI: // 英数レイヤーの Win / Alt (Ctrlのホールドで英数レイヤーが上がったときを含む)
        case KC_LALT:
            /* レイヤーを上げるより先に見る。握りつぶした修飾はホストへ出ないので、
             * 上げてしまうと下ろす機会を失う */
            if (!process_android_gui_alt(keycode, record)) {
                return false;
            }
            process_alpha_gui_alt(keycode, record);
            break; // 修飾そのものはQMKに任せる

        case ALPHA_FN: // 英数レイヤーの Fn。左右とも押している間は設定レイヤーへ (issue #62)
            return process_fn(record);

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

    bottom_tick(timer_read());
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
