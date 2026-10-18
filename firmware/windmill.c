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
 * default_layer_set を使う。ただし、かなレイヤー最下段のタップ・ホールドだけは
 * 自前で判定する (「かなレイヤー最下段のタップ・ホールド」参照)。かな入力は
 * OS側のIMEに任せる (レイヤー0が素のQWERTY+数字段で、標準のJISかな配置になる)。
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
 * が、まだ押している側のぶんまで落としてしまう。 */
#define HOLD_LAYER LAYER_ALPHA

static uint8_t hold_layer_mods      = 0;     // レイヤーを上げている修飾 (MOD_BIT)
static bool    hold_layer_ctrl_seen = false; // Ctrl がホストへ出たのを確認済みか (update_hold_layer 参照)

static void hold_layer_on(uint8_t mod) {
    if (!hold_layer_mods) layer_on(HOLD_LAYER);
    hold_layer_mods |= mod;
}

static void hold_layer_off(uint8_t mod) {
    hold_layer_mods &= ~mod;
    if (mod & MOD_BIT(KC_LEFT_CTRL)) hold_layer_ctrl_seen = false;
    if (!hold_layer_mods) layer_off(HOLD_LAYER);
}

/*
 * MY_LCTL: tap = 言語切替 (英数⇔かな), hold = Ctrl + 英数レイヤー
 *
 * QMK標準の mod-tap (LCTL_T(KC_NO)、windmill.h 参照)。tapping term の計測も、
 * 別キー割り込みでのホールド確定 (HOLD_ON_OTHER_KEY_PRESS) も、Ctrl の
 * 上げ下げも QMK がやる。ここはタップ時の言語切替と、ホールド中だけ英数
 * レイヤーへ移すぶんだけを持つ。Win(つ) / Alt(さ) のホールドと同じ役割。
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
 * 親指Shiftを左に統一した (issue #37) ので、押されている実Shiftは weak Shift と
 * 同じ左Shiftになり、入れ替えは原理的に起こらなくなった。ただしそれに頼って
 * weak Shift を付け直す形へ戻すと、外付けキーボードの右Shift併用など
 * 左Shift以外が押されている場合に同じ罠が復活するので、このままにしておく。 */
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
         * 別キー割り込みでホールドが確定する (HOLD_ON_OTHER_KEY_PRESS) ため、
         * 1打鍵目は
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

static uint8_t tap_hold_tap_keycode(uint16_t keycode);
static bool    is_tap_hold_key(uint16_t keycode);

uint16_t windmill_output_keycode(uint16_t keycode) {
    if (is_tap_hold_key(keycode)) return tap_hold_tap_keycode(keycode);
    return is_key_output_target(keycode) ? get_key_output(keycode).plain : keycode;
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

/* 左右とも ALPHA_THUMB_SHIFT (LSFT_T(KC_SPC)) で同じ左Shiftになる。QMKの修飾は
 * ビットマスクで参照カウントを持たないので、素のままだと先に離したほうの
 * unregister_mods() でShiftが落ち、もう片方を押していてもホストからはShiftが
 * 離れて見える。押しっぱなしのほうが残っているうちは離すイベントごと消費して、
 * 持ち替え (ハンドオーバー) を途切れさせない。
 *
 * キーコードでは左右を区別できないので、windmill.h の説明どおり matrix の列
 * (col) で見分ける (issue #40)。列は機種ごとに違いうるので、値は windmill.h /
 * 機種の config.h が持つ (issue #68)。
 *
 * かなレイヤーの こ / み (KT_KO / KT_MI) は左右で別の Shift にしたので、この
 * 持ち替えの手当ては要らない。英数レイヤーはIMEを介さないので、Shift+タップ
 * (半角スペース) もそのままQMKに任せてよい。 */
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

/* 「も」(KT_MO) をShiftを押しながらタップしたら、半角「?」を送る。かな入力の
 * ままでは打てない記号なので、Symレイヤーの数字・記号と同じ要領で英数へ
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
 *    親指Shiftを左に統一した (issue #37) 今は入れ替えの相手がいないが、
 *    外付けキーボードの右Shiftとの併用まで考えると実Shiftを使うほうが安全。
 *
 * 加えて、モードとShiftの変化はどちらもIMEが非同期に処理するので、レポートを
 * 送るたびにウェイトを挟んで順序を保証する。修飾の上げ下げにも1回ずつ要るのは、
 * Shiftを押した直後に KC_SLSH を送るとIMEが切り替え処理に紛れてShiftを取り
 * こぼすため。del_mods()/set_mods() ではなく register/unregister を使うのは、
 * ホスト側にもShiftの上げ下げを届ける必要があるから (これも issue #18)。
 *
 * タップかどうかの判定は tap_hold_tap() が持つ。ここはShiftが押されているときの
 * 送出だけ */
static void send_kana_qmark(void) {
    const uint8_t shift = get_mods() & MOD_MASK_SHIFT;

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
 * かなベースでの英数レイヤーの Win / Alt
 */

/* Ctrl を先に押していると英数レイヤーが既に上がっていて、つ / さ の位置は
 * かなレイヤーの KT_TSU / KT_SA ではなく英数レイヤー側の素の KC_LGUI / KC_LALT
 * として解決される。つ のホールド中に さ を押したときも同じ。これも数えないと、
 * 先に Ctrl (や つ) を離した時点で英数レイヤーが落ち、Win / Alt を押したまま
 * かなで解決されてしまう (issue #34)。
 *
 * 離すほうは、押したときのレイヤーで引き直される (QMK の source layers cache) ので、
 * 必ず同じ KC_LGUI / KC_LALT として届く。 */
static void process_alpha_gui_alt(uint16_t keycode, keyrecord_t *record) {
    const uint8_t mod = (keycode == KC_LGUI) ? MOD_BIT(KC_LEFT_GUI) : MOD_BIT(KC_LEFT_ALT);

    if (!record->event.pressed) {
        if (hold_layer_mods & mod) hold_layer_off(mod);
        return;
    }
    if (get_highest_layer(default_layer_state) != LAYER_KANA) return; // 英数ベースなら何もしない
    hold_layer_on(mod);
}

/* Ctrl (MY_LCTL) のぶんのレイヤーを下ろすのは、キーの解放イベントではなく
 * **ホストへ出ている修飾**を毎スキャン見て判断する。
 *
 * MY_LCTL は QMK の mod-tap なので、Ctrl の上げ下げは QMK が持っている。解放が
 * どのキーコードで解決されようと、修飾の状態を見るなら必ず戻るし、英数レイヤーの
 * 並びを変えても壊れない。ただしレイヤーを上げた直後は、QMKがまだ修飾を
 * register していないスキャンがありうる。一度ホストへ出たのを確認してから下ろす。
 *
 * Win(つ) / Alt(さ) はかつて同じ形だったが、今はホールドの上げ下げを自前で持つ
 * (tap_hold_start / tap_hold_end) ので、ここでは見ない。 */
static void update_hold_layer(void) {
    if (!(hold_layer_mods & MOD_BIT(KC_LEFT_CTRL))) return;

    if (get_mods() & MOD_MASK_CTRL) {
        hold_layer_ctrl_seen = true;
    } else if (hold_layer_ctrl_seen) {
        hold_layer_off(MOD_BIT(KC_LEFT_CTRL));
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
 * 前者を踏みうる。「つ」のホールド中は英数レイヤーが上がり、「さ」はそこの素の
 * KC_LALT になるため。 */

/* 押下を握りつぶした修飾 (MOD_BIT)。離すイベントも消費するために覚える。
 * 握りつぶしたほうは、先に押したほうを離しても出さない。出すと押した覚えの無い
 * 修飾が遅れて効くことになる */
static uint8_t android_blocked_mods = 0;

/* Win と Alt は後から押したほうをホストへ出さない。英数レイヤーの素の
 * KC_LGUI / KC_LALT と、かなレイヤーの つ / さ のホールド (tap_hold_start) の両方から呼ぶ。
 * falseを返したらそのイベントは消費済み(以降の処理をスキップ) */
static bool process_android_gui_alt(bool is_gui, bool pressed) {
    const uint8_t mod   = is_gui ? MOD_BIT(KC_LEFT_GUI) : MOD_BIT(KC_LEFT_ALT);
    const uint8_t other = is_gui ? MOD_MASK_ALT : MOD_MASK_GUI;

    if (!pressed) {
        // 押下時のOS設定は見ない。押している間に設定を変えても離すぶんは消費する
        if (!(android_blocked_mods & mod)) return true;
        android_blocked_mods &= ~mod;
        return false;
    }
    if (!is_android() || !(get_mods() & other)) return true;

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
 * 押下の中で送り切って、Win を外している間に他のキーの処理を挟まない。
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
 * Fnを左右1ビットずつ覚えて、レイヤーの上げ下げを自前で持つ。
 *
 * かなレイヤーの Fn (そ / ね) はホールドの判定ごと自前なので、Fnレイヤーの上げ下げも
 * ここで持つ (fn_hold_on / fn_hold_off)。英数レイヤーの Fn は QMK の MO() のまま。
 * ただしQMKのレイヤーは参照カウントを持たないので、素のままだと先に離したほうの
 * layer_off() が、まだ押している側のぶんまで落としてしまう。片方が残っているうちは
 * 離すイベントごと消費して、Fnレイヤーへ戻れるようにする (process_fn)。
 *
 * 英数レイヤーでは、設定レイヤーの Fn の位置を必ず透過にしておくこと。2つ目のFnの
 * 押下でここがレイヤーを上げると、QMKはそのキー自身のアクションを上げた後の
 * レイヤーで引き直す。透過でないと解放がFnとして解決されず、設定レイヤーへ
 * 貼りついたまま戻れなくなる。 */
static uint8_t fn_held = 0;

#define FN_BIT_L 1
#define FN_BIT_R 2
#define FN_BOTH  (FN_BIT_L | FN_BIT_R)
#define FN_BIT(record) ((record)->event.key.col == FN_L_COL ? FN_BIT_L : FN_BIT_R)

static void update_conf_layer(void) {
    if (fn_held == FN_BOTH) {
        layer_on(LAYER_CONF);
    } else if (layer_state_is(LAYER_CONF)) {
        layer_off(LAYER_CONF);
    }
}

// かなレイヤーの Fn のホールド。英数レイヤーの Fn と同じビットを使うので、混ぜて押しても数えられる
static void fn_hold_on(uint8_t bit) {
    fn_held |= bit;
    layer_on(LAYER_FN);
    update_conf_layer();
}

static void fn_hold_off(uint8_t bit) {
    fn_held &= ~bit;
    update_conf_layer();
    if (!fn_held) layer_off(LAYER_FN);
}

// 英数レイヤーの Fn (MO(3))。falseを返したらそのイベントは消費済み(以降の処理をスキップ)
static bool process_fn(keyrecord_t *record) {
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

/*
 * かなレイヤー最下段のタップ・ホールド
 */

/* かなレイヤー最下段の8キー (KT_*) は、タップとホールドの判定を QMK の LT() / MT()
 * ではなく自前で持つ (issue #83)。最下段は親指で隣り合ったキーを続けて打つので、
 * 「別のキーを押したらホールド」(HOLD_ON_OTHER_KEY_PRESS) だと、こ→み と速く
 * 打っただけで こ が Shift になる。かといって時間だけで決めると、こ を押してすぐ
 * 打った文字に Shift が乗らない。そこで次のように決める。
 *
 * - ホールドに決まるのは、押してから TAP_HOLD_TERM_MS たったとき (単独の長押しを
 *   含む)、または TAP_HOLD_INTERRUPT_MS 以上たっていて、後から別のキーが押されて
 *   いるとき。どちらも「以上」
 * - タップに決まるのは、未判定のまま離したとき
 * - 最下段のキーの押下では、先に押された未判定のキーをその場では判定しない。
 *   最下段以外のキーの押下は、TAP_HOLD_INTERRUPT_MS 以上たった未判定のキーを
 *   その場でホールドに決めてから処理する。決めずに通すと、そのキーはホールドで
 *   上がるはずのレイヤーや修飾の手前で解決されてしまう
 * - 時間の判定はスキャンごとに行う (matrix_scan_kb)
 *
 * 未判定のキー K が押されてから TAP_HOLD_INTERRUPT_MS 未満に押されたキーは、K が
 * 決まるまで出力を待たせる。K がホールドになればその修飾やレイヤーで、タップなら
 * 素のかなレイヤーで解決したいため。そのためキーイベントを pre_process_record_kb()
 * で横取りして列に溜め、K が決まってから action_exec() で流し直す。キーコードは
 * 流し直した時点で引かれるので、待っていたキーは K が決まった時点のレイヤーと
 * 修飾で解決される。ただし K の相方がホールド中なら待たせない。相方の Shift を
 * 押したまま こ / み を打ち始めたときに、文字が遅れないようにするため。
 *
 * 待たせるのは押下だけだが、押した順を保つため、溜めているものがある間は後続の
 * イベントを全て列の後ろに並べる。最下段のキー自身もこの列に並ぶ。判定 (時間と
 * 離したかどうか) は実際の押し離しで進め、タップの出力やホールドの開始は列の
 * 順番が来てから行う。
 *
 * タップは判定が付いたときに押し離しを送り切るので、最下段のキーはリピートしない。 */
#define TAP_HOLD_INTERRUPT_MS 150
#define TAP_HOLD_TERM_MS      250

/* 同時に追うキーと、溜めておくイベントの数。最下段は8キーなので、追うほうは
 * 押し直しの重なりを見込んでも余る。列は足りなくなったら、先頭を待たずに流す */
#define TAP_HOLD_SLOTS 16
#define TAP_HOLD_QUEUE 16

enum {
    TAP_HOLD_UNDECIDED,
    TAP_HOLD_TAP,
    TAP_HOLD_HOLD,
    /* 列で待っている間にレイヤーが変わり、順番が来たときには最下段のキーで
     * なくなっていたもの。そのキーの押し離しは QMK へ流す */
    TAP_HOLD_PASS,
};

typedef struct {
    keypos_t key;
    uint16_t keycode;    // KT_*。KC_NO なら空き
    uint16_t pressed_at;
    uint16_t later_at;   // 後から押された別のキーのうち、最初のものの時刻
    uint16_t hold_at;    // ホールドの条件が満たされた時刻。判定したスキャンの時刻ではない
    uint8_t  state : 2;  // TAP_HOLD_*
    bool     has_later : 1;
    bool     released : 1;
    bool     live : 1;   // 列の順番が来た。判定が付いたらその場で出力する
} tap_hold_t;

typedef struct {
    keyevent_t event;
    int8_t     slot; // 最下段のキーなら tap_holds[] の添字。それ以外は -1
} tap_hold_event_t;

static tap_hold_t       tap_holds[TAP_HOLD_SLOTS];
static tap_hold_event_t tap_hold_queue[TAP_HOLD_QUEUE];
static uint8_t          tap_hold_queued    = 0;
static bool             tap_hold_replaying = false; // 列から流し直している最中

/* こ / み のホールド (Shift) 開始後に他のキーが押されたらダーティ。
 * 相方の Shift 中にタップした こ / み は半角スペースになるが、ダーティなら
 * 誤入力ガードとして何も出さない */
static bool thumb_shift_dirty = false;

// 記号レイヤーを上げている ひ / も。片方を離しても、もう片方が残っていれば下ろさない
static uint8_t sym_held = 0;

#define SYM_BIT(keycode) ((keycode) == KT_HI ? 1 : 2)

static bool is_tap_hold_key(uint16_t keycode) {
    return keycode >= TAP_HOLD_FIRST && keycode <= TAP_HOLD_LAST;
}

// タップで送るキー。タップの出し方そのものは QMK の LT() / MT() だったころと同じ
static const uint8_t PROGMEM tap_hold_taps[] = {
    [KT_TSU - TAP_HOLD_FIRST] = KC_Z,
    [KT_SA - TAP_HOLD_FIRST]  = KC_X,
    [KT_SO - TAP_HOLD_FIRST]  = KC_C,
    [KT_HI - TAP_HOLD_FIRST]  = KC_V,
    [KT_KO - TAP_HOLD_FIRST]  = KC_B,
    [KT_MI - TAP_HOLD_FIRST]  = KC_N,
    [KT_MO - TAP_HOLD_FIRST]  = KC_M,
    [KT_NE - TAP_HOLD_FIRST]  = KC_COMMA,
};
_Static_assert(ARRAY_SIZE(tap_hold_taps) == TAP_HOLD_LAST - TAP_HOLD_FIRST + 1, "tap_hold_taps[] の行が KT_* の数と合っていない");

static uint8_t tap_hold_tap_keycode(uint16_t keycode) {
    return pgm_read_byte(&tap_hold_taps[keycode - TAP_HOLD_FIRST]);
}

// 同じ役割の組の、もう片方。つ / さ には無い
static uint16_t tap_hold_partner(uint16_t keycode) {
    switch (keycode) {
        case KT_KO: return KT_MI;
        case KT_MI: return KT_KO;
        case KT_SO: return KT_NE;
        case KT_NE: return KT_SO;
        case KT_HI: return KT_MO;
        case KT_MO: return KT_HI;
    }
    return KC_NO;
}

/* a が b と同時かそれより後か。どちらも少し前の時刻であること。16bitのタイマーは
 * 1分ほどで一周するので、値の大小ではなく今からの距離で比べる */
static bool is_at_or_after(uint16_t a, uint16_t b) {
    const uint16_t now = timer_read();
    return TIMER_DIFF_16(now, a) <= TIMER_DIFF_16(now, b);
}

// 相方がホールド中ならそれを返す。ホールド中とは、ホールドを開始していて、まだ終えていないこと
static const tap_hold_t *holding_partner(const tap_hold_t *th) {
    const uint16_t partner = tap_hold_partner(th->keycode);
    if (partner == KC_NO) return NULL;

    for (uint8_t i = 0; i < TAP_HOLD_SLOTS; ++i) {
        const tap_hold_t *p = &tap_holds[i];
        if (p->keycode == partner && p->live && p->state == TAP_HOLD_HOLD) return p;
    }
    return NULL;
}

/* こ / み のタップのうち、相方の Shift 中のもの。半角スペースを送る。
 *
 * 実Shiftを外してから送る。以前は del_mods()/set_mods() で real_mods を書き換えて
 * いた。この2つはレポートを送らないので、ホストへ届く1本目が
 *
 *   [LSFT] (ホールド確定) → [KC_SPC]
 *
 * と、「Shiftを離す」と「Spaceを押す」を1本にまとめた形になり、IMEがShiftの上げ
 * 下げを追い切れずに1打鍵目だけShiftが乗ったままになっていた。process_shift_pair()
 * の「Shiftを外して送る」経路と同じ症状 (issue #36)。register系でShiftを外す
 * レポートを1本独立させ、あわせてそちらと同じウェイトを挟んで順序を保証する。 */
static void send_thumb_space(void) {
    const uint8_t shift = get_mods() & MOD_MASK_SHIFT;

    wait_ms(IME_WAIT_MS);
    unregister_mods(shift);
    wait_ms(IME_WAIT_MS);
    tap_code16(KC_SPC);
    wait_ms(IME_WAIT_MS);
    register_mods(shift);
}

static void tap_hold_tap(const tap_hold_t *th) {
    switch (th->keycode) {
        case KT_KO:
        case KT_MI: {
            /* 相方の Shift が始まってから押したものだけをスペースにする。相方が
             * 未判定のうちに押したもの (こ+み を続けて打った) は、こ / み のまま */
            const tap_hold_t *partner = holding_partner(th);
            if (partner && is_at_or_after(th->pressed_at, partner->hold_at)) {
                if (!thumb_shift_dirty) send_thumb_space();
                return; // Shift 開始後に他のキーを押していたら何も出さない
            }
            break;
        }
        case KT_MO:
            if (get_mods() & MOD_MASK_SHIFT) {
                send_kana_qmark();
                return;
            }
            break;
    }
    tap_code(tap_hold_tap_keycode(th->keycode)); // 押されている Shift はそのまま乗る
}

static void tap_hold_start(const tap_hold_t *th) {
    switch (th->keycode) {
        case KT_KO:
        case KT_MI:
            /* 左右で別の Shift にする。同じ Shift だとQMKの修飾ビットを共有するので、
             * 持ち替えで先に離したほうが、まだ押している側の Shift まで落としてしまう */
            thumb_shift_dirty = false;
            register_mods(th->keycode == KT_KO ? MOD_BIT(KC_LEFT_SHIFT) : MOD_BIT(KC_RIGHT_SHIFT));
            break;
        case KT_TSU:
        case KT_SA: {
            const bool    is_gui = th->keycode == KT_TSU;
            const uint8_t mod    = is_gui ? MOD_BIT(KC_LEFT_GUI) : MOD_BIT(KC_LEFT_ALT);
            /* レイヤーを上げるより先に見る。握りつぶした修飾はホストへ出ないので、
             * レイヤーも上げない */
            if (!process_android_gui_alt(is_gui, true)) break;
            register_mods(mod);
            hold_layer_on(mod);
            break;
        }
        case KT_SO:
        case KT_NE:
            fn_hold_on(th->keycode == KT_SO ? FN_BIT_L : FN_BIT_R);
            break;
        case KT_HI:
        case KT_MO:
            if (!sym_held) layer_on(LAYER_SYM);
            sym_held |= SYM_BIT(th->keycode);
            break;
    }
}

static void tap_hold_end(const tap_hold_t *th) {
    switch (th->keycode) {
        case KT_KO:
        case KT_MI:
            unregister_mods(th->keycode == KT_KO ? MOD_BIT(KC_LEFT_SHIFT) : MOD_BIT(KC_RIGHT_SHIFT));
            break;
        case KT_TSU:
        case KT_SA: {
            const bool    is_gui = th->keycode == KT_TSU;
            const uint8_t mod    = is_gui ? MOD_BIT(KC_LEFT_GUI) : MOD_BIT(KC_LEFT_ALT);
            if (!process_android_gui_alt(is_gui, false)) break; // 握りつぶしていたもの
            unregister_mods(mod);
            hold_layer_off(mod);
            break;
        }
        case KT_SO:
        case KT_NE:
            fn_hold_off(th->keycode == KT_SO ? FN_BIT_L : FN_BIT_R);
            break;
        case KT_HI:
        case KT_MO:
            sym_held &= ~SYM_BIT(th->keycode);
            if (!sym_held) layer_off(LAYER_SYM);
            break;
    }
}

// 判定を付ける。列の順番が来ているキーなら、その場で出力する
static void tap_hold_decide(tap_hold_t *th, uint8_t state, uint16_t at) {
    th->state   = state;
    th->hold_at = at;
    if (!th->live) return;

    if (state == TAP_HOLD_TAP) {
        tap_hold_tap(th);
    } else {
        tap_hold_start(th);
    }
}

/* 時間でホールドに決まるものを決める。出力の順が押した順になるよう、先に押された
 * ものから決める。後から押されたキーが決まるときは、先に押されたほうも必ず条件を
 * 満たしているので (後から押されたキーそのものが「後から押された別のキー」になる)、
 * 古い順に条件を見ていけば足りる */
static void tap_hold_check_time(uint16_t now) {
    for (;;) {
        tap_hold_t *next   = NULL;
        uint16_t    offset = 0;
        for (uint8_t i = 0; i < TAP_HOLD_SLOTS; ++i) {
            tap_hold_t *th = &tap_holds[i];
            if (th->keycode == KC_NO || th->state != TAP_HOLD_UNDECIDED) continue;

            uint16_t needed = TAP_HOLD_TERM_MS;
            if (th->has_later) {
                // 後から押されたのが TAP_HOLD_INTERRUPT_MS より後なら、条件が揃うのはその押下の時点
                const uint16_t later = TIMER_DIFF_16(th->later_at, th->pressed_at);
                needed               = MIN(MAX(later, TAP_HOLD_INTERRUPT_MS), TAP_HOLD_TERM_MS);
            }
            if (TIMER_DIFF_16(now, th->pressed_at) < needed) continue;
            if (next && is_at_or_after(th->pressed_at, next->pressed_at)) continue;
            next   = th;
            offset = needed;
        }
        if (!next) return;
        tap_hold_decide(next, TAP_HOLD_HOLD, next->pressed_at + offset);
    }
}

/* この時刻に押されたキーを待たせるか。順番が来ている未判定のキーのうち、
 * 押されてから TAP_HOLD_INTERRUPT_MS 未満のものがあれば待たせる。
 * ただしそのキーの相方がホールド中なら待たせない */
static bool tap_hold_must_wait(uint16_t time) {
    for (uint8_t i = 0; i < TAP_HOLD_SLOTS; ++i) {
        const tap_hold_t *th = &tap_holds[i];
        if (th->keycode == KC_NO || !th->live || th->state != TAP_HOLD_UNDECIDED) continue;
        if (TIMER_DIFF_16(time, th->pressed_at) >= TAP_HOLD_INTERRUPT_MS) continue;
        if (holding_partner(th)) continue;
        return true;
    }
    return false;
}

static void tap_hold_replay(keyevent_t event) {
    tap_hold_replaying = true;
    action_exec(event);
    tap_hold_replaying = false;
}

// 列の順番が来たイベントを処理する
static void tap_hold_dispatch(tap_hold_event_t entry) {
    if (entry.slot < 0) {
        tap_hold_replay(entry.event);
        return;
    }

    tap_hold_t *th = &tap_holds[entry.slot];
    if (!entry.event.pressed) {
        if (th->state == TAP_HOLD_PASS) {
            tap_hold_replay(entry.event);
        } else if (th->state == TAP_HOLD_HOLD) {
            tap_hold_end(th);
        }
        th->keycode = KC_NO; // タップはもう送ってある
        return;
    }

    // 待っている間に上がったレイヤーで引き直すと、最下段のキーではなくなっていることがある
    const keypos_t key = entry.event.key;
    if (!is_tap_hold_key(keymap_key_to_keycode(layer_switch_get_layer(key), key))) {
        th->state = TAP_HOLD_PASS; // 離すほうも QMK へ流す
        tap_hold_replay(entry.event);
        return;
    }

    th->live = true;
    if (th->keycode != KT_KO && th->keycode != KT_MI) thumb_shift_dirty = true;

    if (th->state == TAP_HOLD_TAP) {
        tap_hold_tap(th);
    } else if (th->state == TAP_HOLD_HOLD) {
        tap_hold_start(th);
    }
}

// 先頭から、待たなくてよくなったものを流す
static void tap_hold_drain(void) {
    while (tap_hold_queued) {
        const tap_hold_event_t head = tap_hold_queue[0];
        if (head.event.pressed && tap_hold_must_wait(head.event.time)) return;

        memmove(&tap_hold_queue[0], &tap_hold_queue[1], sizeof(tap_hold_queue[0]) * --tap_hold_queued);
        tap_hold_dispatch(head);
    }
}

static void tap_hold_enqueue(keyevent_t event, int8_t slot) {
    if (tap_hold_queued == TAP_HOLD_QUEUE) { // 溢れるなら、先頭を待たずに流して空ける
        const tap_hold_event_t head = tap_hold_queue[0];
        memmove(&tap_hold_queue[0], &tap_hold_queue[1], sizeof(tap_hold_queue[0]) * --tap_hold_queued);
        tap_hold_dispatch(head);
    }
    tap_hold_queue[tap_hold_queued++] = (tap_hold_event_t){.event = event, .slot = slot};
}

static int8_t tap_hold_alloc(keyevent_t event, uint16_t keycode) {
    for (uint8_t i = 0; i < TAP_HOLD_SLOTS; ++i) {
        if (tap_holds[i].keycode != KC_NO) continue;
        tap_holds[i] = (tap_hold_t){.key = event.key, .keycode = keycode, .pressed_at = event.time};
        return i;
    }
    return -1;
}

// その位置で押されていて、まだ離されていないキー
static int8_t tap_hold_find_pressed(keypos_t key) {
    for (uint8_t i = 0; i < TAP_HOLD_SLOTS; ++i) {
        const tap_hold_t *th = &tap_holds[i];
        if (th->keycode != KC_NO && !th->released && KEYEQ(th->key, key)) return i;
    }
    return -1;
}

/* すべてのキーイベントの入口 (pre_process_record_kb)。falseを返したら、イベントは
 * ここで引き取った (自前で処理したか、列に並べた) */
static bool process_tap_hold(uint16_t keycode, keyrecord_t *record) {
    const keyevent_t event = record->event;
    if (!IS_KEYEVENT(event)) return true;

    int8_t slot = -1;
    if (event.pressed) {
        for (uint8_t i = 0; i < TAP_HOLD_SLOTS; ++i) {
            tap_hold_t *th = &tap_holds[i];
            if (th->keycode == KC_NO || th->state != TAP_HOLD_UNDECIDED || th->has_later) continue;
            th->has_later = true;
            th->later_at  = event.time;
        }
        if (is_tap_hold_key(keycode)) {
#ifdef WINDMILL_LED_ENABLE
            refresh_led_timeout(); // QMK を通らないので、点灯はここで。キーはそのまま打てる
#endif
            slot = tap_hold_alloc(event, keycode);
            if (slot < 0) return false; // 追いきれない。押し離しごと捨てる
        } else {
            tap_hold_check_time(event.time); // このキーより先に、ホールドに決まるものを決める
        }
    } else {
        slot = tap_hold_find_pressed(event.key);
        if (slot >= 0) {
            tap_hold_t *th = &tap_holds[slot];
            th->released   = true;
            if (th->state == TAP_HOLD_PASS) {
                th->keycode = KC_NO; // 押下を QMK へ流したので、離すほうも素のイベントとして扱う
                slot        = -1;
            } else if (th->state == TAP_HOLD_UNDECIDED) {
                tap_hold_decide(th, TAP_HOLD_TAP, event.time);
            }
        } else if (is_tap_hold_key(keycode)) {
            return false; // 押下を捨てたもの
        }
    }

    if (!tap_hold_queued && (!event.pressed || !tap_hold_must_wait(event.time))) {
        if (slot < 0) return true; // 待たなくてよい。そのまま QMK へ
        tap_hold_dispatch((tap_hold_event_t){.event = event, .slot = slot});
        return false;
    }
    tap_hold_enqueue(event, slot);
    tap_hold_drain();
    return false;
}

/*
 * QMK callbacks
 */

bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // 列から流し直すイベントは、届いたときに一度ここを通っている
    if (!tap_hold_replaying) {
        windmill_board_pre_process_record(keycode, record);
        if (!process_tap_hold(keycode, record)) return false;
    }
    return pre_process_record_user(keycode, record);
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
#ifdef WINDMILL_LED_ENABLE
    if (!process_led_timeout(keycode, record)) return false;
#endif
    if (!process_record_user(keycode, record)) return false;

    // かなレイヤー最下段のキー (KT_*) はここへ来ないので、どのキーの押下でもダーティ化
    if (record->event.pressed) {
        thumb_shift_dirty = true;
    }

    /* KT_* は process_tap_hold() が引き取るので、ふつうは来ない。待っている間に
     * レイヤーが下りて、順番が来たときに初めて KT_* として引かれたときだけ来る。
     * 押下を自前で追っていないので、何もしない */
    if (is_tap_hold_key(keycode)) return false;

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

        case KC_LGUI: // 英数レイヤー側の Win / Alt (Ctrlと重ね押ししたとき)
        case KC_LALT:
            /* レイヤーを上げるより先に見る。握りつぶした修飾はホストへ出ないので、
             * レイヤーも上げない */
            if (!process_android_gui_alt(keycode == KC_LGUI, record->event.pressed)) {
                return false;
            }
            process_alpha_gui_alt(keycode, record);
            break; // 修飾そのものはQMKに任せる

        case ALPHA_FN: // 英数レイヤーのFn (左右とも同じキーコード)。両方押している間は設定レイヤーへ (issue #62)
            if (!process_fn(record)) {
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

    // かなレイヤー最下段のキーの、時間でのホールドの判定と、待たせていたイベントの送出
    tap_hold_check_time(timer_read());
    tap_hold_drain();

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
