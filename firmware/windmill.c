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

/* Windmill 共通処理。かな入力はOS側のIMEに任せ、かなのキーは JISかな配列で
 * そのかなが載っている位置のキーを送る */

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

/*
 * 接続先ごとの設定
 */

/* 接続先のOS。言語切替と「」は、IMEの実装で要るキーが違う */
#define OS_WIN  0 // Windows/デスクトップ
#define OS_ANDR 1 // Android
// 2, 3 は予約 (Mac / iOS 用)。読めても Windows と同じに扱う

/* 無線機は接続先ごとに相手が違うので、OSと配列 (WINDMILL_LAYOUT_*) を接続先ごとに
 * 2bitずつ覚える (issue #58)。0 を既定 (Windows / JIS) にしてあるのは、EEPROMリセット
 * 直後も、配列の設定が入る前のファームウェアで使っていた機体も、0 のまま起動するから */
enum {
    HOST_OS,
    HOST_LAYOUT,
    HOST_SETTING_SIZE,
};
#define HOST_BITS 2
#define HOST_MASK ((1u << HOST_BITS) - 1)

typedef union {
    uint32_t raw;
    struct {
        bool     reserved : 1;     // 予約。旧形式のAndroid設定の跡で、後ろの項目の位置を変えないために残す
        bool     led_darkmode : 1; // LEDを暗いほうの配色にする (LED非搭載機では未使用)
        uint32_t host : HOST_SETTING_SIZE * WINDMILL_HOST_SIZE * HOST_BITS; // HOST_* ごとに、接続先の数だけ並ぶ
    };
} windmill_config_t;
_Static_assert(sizeof(windmill_config_t) == sizeof(uint32_t), "windmill_config_t が eeconfig_kb の32bitに収まっていない");
static windmill_config_t windmill_config;

/* 範囲外が来てもEEPROMの隣の接続先を壊さないよう、有線扱いに丸める */
static uint8_t current_host(void) {
    const uint8_t host = windmill_board_host();
    return host < WINDMILL_HOST_SIZE ? host : WINDMILL_HOST_USB;
}

static uint8_t host_shift(uint8_t setting) {
    return (setting * WINDMILL_HOST_SIZE + current_host()) * HOST_BITS;
}

/* 接続先の切り替えを追わずに済むよう、キャッシュせず毎回引く */
static uint8_t get_host_setting(uint8_t setting) {
    return (windmill_config.host >> host_shift(setting)) & HOST_MASK;
}

// いま繋がっている接続先のぶんだけ書き換える
static void set_host_setting(uint8_t setting, uint8_t value) {
    const uint8_t  shift = host_shift(setting);
    const uint32_t next  = (windmill_config.host & ~((uint32_t)HOST_MASK << shift)) | ((uint32_t)(value & HOST_MASK) << shift);
    if (windmill_config.host == next) return; // 無変更ならEEPROMを書かない
    windmill_config.host = next;
    eeconfig_update_kb(windmill_config.raw);
}

static uint8_t get_host_os(void) {
    return get_host_setting(HOST_OS);
}

static bool is_android(void) {
    return get_host_os() == OS_ANDR;
}

uint8_t windmill_host_layout(void) {
    const uint8_t layout = get_host_setting(HOST_LAYOUT);
    return layout == WINDMILL_LAYOUT_US ? WINDMILL_LAYOUT_US : WINDMILL_LAYOUT_JIS; // 予約の値は既定に倒す
}

/*
 * 起動時のベースレイヤー
 */

/* 英数/かなはEEPROMに保存せず、起動のたびに英数から始める。eeconfig_init_kb() は
 * EEPROMの初期化直後にしか呼ばれないので keyboard_post_init_kb() から呼ぶ (issue #22)。
 * default_layer_set() は eeconfig も書いてしまうので、直接書き換える */
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
    if (IS_QK_MOD_TAP(keycode)) return QK_MOD_TAP_GET_TAP_KEYCODE(keycode);
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

/* 有効なレイヤーを上から見て、透過でない最初の色を採用する (QMKのキーコード解決と同じ)。
 * default_layer_state_set_kb() からは更新後の値を渡すので、グローバルを読まない */
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

    // RGB Matrix は rgb_matrix_indicators_kb() で毎フレーム描き直す
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

/* かな中でも Ctrl+C や Win+V がかな配列のまま解決されないようにする (issue #34)。
 *
 * 上げ下げは、押されている修飾 (mods) から決める。修飾ごとに数えて上げ下げすると、
 * 解放を取り逃したときにレイヤーが残ってしまうし、QMKのレイヤーは参照カウントを
 * 持たないので、重ね押しで先に離したほうがまだ押している側のぶんまで落とす。
 * 英数ベースで上げても見え方は変わらないので、ベースレイヤーは見ない */
#define HOLD_LAYER      LAYER_ALPHA
#define HOLD_LAYER_MODS (MOD_MASK_CTRL | MOD_MASK_GUI | MOD_MASK_ALT)

// mods は、このイベントを処理した後に押されている修飾。ホストへ出す前に決めてもよい
static void set_hold_layer(uint8_t mods) {
    const bool on = mods & HOLD_LAYER_MODS;
    if (on == layer_state_is(HOLD_LAYER)) return; // 塗り直しを減らす
    if (on) {
        layer_on(HOLD_LAYER);
    } else {
        layer_off(HOLD_LAYER);
    }
}

/*
 * MY_LCTL: tap = 言語切替 (英数⇔かな), hold = Ctrl + 英数レイヤー
 *
 * タップ・ホールドの判定は最下段のキーと同じ (「かなレイヤー最下段のタップ・ホールド」、
 * issue #95)。QMK の mod-tap だと、最下段のキーが未判定の間に押した Ctrl が押した順で
 * 効かず、境目も tapping term で最下段と揃わない。
 */

/* タップのたびに英数⇔かなを入れ替える。キーボード側はIMEの状態を読めないので、
 * ベースレイヤーを自分で反転させて追う。
 *
 * - Windows: KC_LNG1 / KC_LNG2 でモードを直接指定する。トグルのキーと違い、IME側が
 *   ずれてもタップし直せば揃う
 * - Android: 日本語IMEは入力欄を移るたびにかなへ戻ってしまうので、Ctrl+Space で
 *   英語IMEと入れ替える (issue #53)。トグルなので、ずれたら MY_IME で合わせ直す */
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

// default_layer_set はEEPROMを書かないので頻繁な切り替えでも安全
static void switch_lang(void) {
    const uint8_t next = get_highest_layer(default_layer_state) == LAYER_KANA ? LAYER_ALPHA : LAYER_KANA;

    send_lang_to_host(next);
    default_layer_set((layer_state_t)1 << next);
}

/*
 * 文字キーの出力表
 */

/* 文字キー (KN_* / SY_*) と最下段のタップ (KB_*) でホストへ送るキー (issue #73)。
 * 同じ文字でも送るキーは接続先の配列で変わる (「@」は US なら Shift+2、JIS なら単独の
 * キー) ので、キーマップは「どの文字のキーか」だけを持ち、違いはここで吸収する。
 *
 * 基準を Windows + JIS にしたのは、かなの刻印がそのまま JIS 配列の位置だから。
 * Android (Gboard) の JIS も同じ表を引く。AOSP の Generic.kl では ろ (KC_INT1) と
 * ¥ (KC_INT3) も専用のキーで、Windows と違うと分かっているキーは無い */

#define ROW(keycode) ((keycode) - KEY_OUTPUT_FIRST)

static const uint16_t PROGMEM key_outputs[] = {
    // かなレイヤー最下段のタップ
    [ROW(KB_TSU)]      = KC_Z,
    [ROW(KB_SA)]       = KC_X,
    [ROW(KB_SO)]       = KC_C,
    [ROW(KB_HI)]       = KC_V,
    [ROW(KB_KO)]       = KC_B,
    [ROW(KB_MI)]       = KC_N,
    [ROW(KB_MO)]       = KC_M,
    [ROW(KB_NE)]       = JP_COMM,
    [ROW(KB_XTSU)]     = S(KC_Z),
    [ROW(KB_SPC)]      = KC_SPC,
    [ROW(KB_QUES)]     = JP_QUES,
    [ROW(KB_TOUTEN)]   = S(JP_COMM),

    // かな
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

    // かなの Shift 面。IMEが同じキーの Shift で出すもの (ぁ、を など) は S() 付き
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
    [ROW(KN_CHOON)]    = JP_YEN,

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

#define OS_ANY 0xFF // 例外の、対象OSを問わない行

/* key_outputs[] の例外。上から見て、キー・配列・OSが当たった最初の行を使う。
 * US 配列でもかなは JISかな配列の位置で出るので、JIS と位置が違う記号のキーに
 * 載っているかなだけ行がある。「」は US のとき、Microsoft IME は [ ] の Shift、
 * Gboard は ] \ の Shift で出る */
static const struct {
    uint16_t keycode;
    uint8_t  layout; // WINDMILL_LAYOUT_*
    uint8_t  os;     // OS_* か OS_ANY
    uint16_t output;
} PROGMEM key_output_exceptions[] = {
    {KN_LKAKKO, WINDMILL_LAYOUT_US, OS_ANDR, S(KC_RBRC)}, // 「
    {KN_RKAKKO, WINDMILL_LAYOUT_US, OS_ANDR, S(KC_BSLS)}, // 」
    {KN_LKAKKO, WINDMILL_LAYOUT_US, OS_ANY, S(KC_LBRC)},
    {KN_RKAKKO, WINDMILL_LAYOUT_US, OS_ANY, S(KC_RBRC)},
    {KN_HE, WINDMILL_LAYOUT_US, OS_ANY, S(KC_EQL)},       // へ
    {KN_MU, WINDMILL_LAYOUT_US, OS_ANY, KC_BSLS},         // む
    {KN_CHOON, WINDMILL_LAYOUT_US, OS_ANY, S(KC_MINS)},   // ー
    {KN_RO, WINDMILL_LAYOUT_US, OS_ANY, KC_GRV},          // ろ
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

static bool is_key_output_target(uint16_t keycode) {
    return keycode >= KEY_OUTPUT_FIRST && keycode <= KEY_OUTPUT_LAST;
}

// keycode は KEY_OUTPUT_FIRST 〜 KEY_OUTPUT_LAST であること
static uint16_t get_key_output(uint16_t keycode) {
    const uint8_t layout = windmill_host_layout();
    const uint8_t os     = get_host_os();

    for (uint8_t i = 0; i < ARRAY_SIZE(key_output_exceptions); ++i) {
        if (pgm_read_word(&key_output_exceptions[i].keycode) != keycode) continue;
        if (pgm_read_byte(&key_output_exceptions[i].layout) != layout) continue;
        const uint8_t row_os = pgm_read_byte(&key_output_exceptions[i].os);
        if (row_os != OS_ANY && row_os != os) continue;
        return pgm_read_word(&key_output_exceptions[i].output);
    }
    return pgm_read_word(&key_outputs[keycode - KEY_OUTPUT_FIRST]);
}

uint16_t windmill_output_keycode(uint16_t keycode) {
    return is_key_output_target(keycode) ? get_key_output(keycode) : keycode;
}

/*
 * 文字キーの送り方
 */

/* IMEは修飾の上げ下げも英数/かなの切り替えも非同期に処理するので、レポートを続けて
 * 送ると前のぶんを取りこぼす (issue #36)。効かなければ増やし、もたつくなら減らす */
#define IME_WAIT_MS 10

// 出力に Shift が要るか (S(KC_x))
static bool is_shifted_keycode(uint16_t keycode) {
    return IS_QK_MODS(keycode) && (QK_MODS_GET_MODS(keycode) & MOD_LSFT);
}

/* 英数に切り替えて送る文字か。記号レイヤーの数字・記号と、Shift 面の半角「？」は
 * かな入力のままでは打てない */
static bool is_alpha_output(uint16_t keycode) {
    if (get_highest_layer(default_layer_state) != LAYER_KANA) return false;
    return (keycode >= SY_1 && keycode <= SY_QUES) || keycode == KB_QUES;
}

/* Shift を from から to へ移し、変わるときは前後にウェイトを挟む。挟まないとIMEが
 * 追い切れず、外した Shift が次のキーに乗ったり、足した Shift が落ちたりする (issue #87)。
 * register 系を使うのは、ホストにも上げ下げを届けるため (issue #18) */
static void set_shift(uint8_t from, uint8_t to) {
    if (from == to) return;
    wait_ms(IME_WAIT_MS);
    if (from) unregister_mods(from);
    if (to) register_mods(to);
    wait_ms(IME_WAIT_MS);
}

/* 文字を1つ、押下と解放を続けて送り切る。押されている Shift は打ち消し、出力に
 * Shift が要るときだけ足す。
 *
 * Shift が要るとき押されている Shift をそのまま使うのは、左Shiftへ付け替えると
 * 「右Shiftを離して左Shiftを押す」レポートが挟まり、そこだけ Shift が効かない機種が
 * あるため。
 *
 * alpha なら英数に切り替えて送り、かなに戻す。切り替えのキーに Shift が乗っていると
 * IMEが英数/かなキーとして受け取らないので、その間は押されている Shift も外す */
static void send_output(uint16_t output, bool alpha) {
    const uint8_t held  = get_mods() & MOD_MASK_SHIFT;
    const uint8_t shift = is_shifted_keycode(output) ? (held ? held : MOD_BIT(KC_LEFT_SHIFT)) : 0;

    clear_weak_mods(); // 残っていれば送るレポートに乗ってしまう

    if (alpha) {
        set_shift(held, 0);
        tap_code(KC_LNG2);
        wait_ms(IME_WAIT_MS); // モードの切り替えも非同期。待たないと全角のまま入る
        set_shift(0, shift);
    } else {
        set_shift(held, shift);
    }

    tap_code(QK_MODS_GET_BASIC_KEYCODE(output));

    if (alpha) {
        set_shift(shift, 0);
        wait_ms(IME_WAIT_MS);
        tap_code(KC_LNG1);
        set_shift(0, held);
    } else {
        set_shift(shift, held);
    }
}

/* 文字キー (KN_* / SY_*) を送る。常に false (QMKには渡さない)。
 * 機種側の process_record にも素のキーコードで渡す。geonix41 のブロブがこれらのキーを見ている */
static bool process_char_key(uint16_t keycode, keyrecord_t *record) {
    const uint16_t output = get_key_output(keycode);
    if (!windmill_board_process_record(QK_MODS_GET_BASIC_KEYCODE(output), record)) return false;

    if (record->event.pressed) send_output(output, is_alpha_output(keycode));
    return false;
}

/*
 * 英数レイヤーの記号 (JIS)
 */

/* 英数レイヤーの ; ' \ は、JIS として受け取られると別の文字が出る (KC_QUOT が「:」)。
 * JIS のときだけ横取りし、押されている Shift で送るものを決めて1キー1出力で送る
 * (issue #80)。Shift の要否が US と食い違うので send_output() に打ち消させ、そのぶん
 * リピートしない。
 *
 * 「\」に ¥ のキー (KC_INT3) ではなく ろ のキー (KC_INT1) を使うのは、¥ のキーは
 * 円記号を出す環境 (macOS の JIS など) があるため */
static const struct {
    uint16_t keycode; // 英数レイヤーの素のキーコード (LT() はタップ側)
    uint16_t plain;   // Shift 無しで送るもの (JIS)
    uint16_t shifted; // Shift 時に送るもの (JIS)
} PROGMEM jis_alpha_symbols[] = {
    {KC_SCLN, JP_SCLN, JP_COLN}, // ; :
    {KC_QUOT, JP_QUOT, JP_DQUO}, // ' "
    {KC_BSLS, JP_BSLS, JP_PIPE}, // \ |
};

/* 横取りしたら false。機種側の process_record には横取り前のキーコードで渡す。
 * 送るものは押下時の Shift で決まるので、送るキーコードでは押下と解放が食い違いうる */
static bool process_jis_alpha_symbol(uint16_t keycode, keyrecord_t *record) {
    uint16_t basic = keycode;
    if (IS_QK_LAYER_TAP(keycode)) {
        if (!record->tap.count) return true; // ホールド (記号レイヤー) はQMKに任せる
        basic = QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
    }
    if (windmill_host_layout() != WINDMILL_LAYOUT_JIS) return true;

    for (uint8_t i = 0; i < ARRAY_SIZE(jis_alpha_symbols); ++i) {
        if (pgm_read_word(&jis_alpha_symbols[i].keycode) != basic) continue;
        if (!windmill_board_process_record(keycode, record)) return false;
        if (record->event.pressed) {
            const bool shifted = get_mods() & MOD_MASK_SHIFT;
            send_output(pgm_read_word(shifted ? &jis_alpha_symbols[i].shifted : &jis_alpha_symbols[i].plain), false);
        }
        return false;
    }
    return true;
}

/*
 * Android での Win / Alt
 */

/* Android は Win (Meta) と Alt の押し離しの順序に反応する (issue #57)。
 *
 * - Win と Alt を重ねると、どちらかを離した時点で Caps Lock が切り替わる
 * - Win を、他のキーを挟まずに離すと、アプリ一覧が開く
 *
 * つ / さ は隣り合っているので、続けて打つだけで前者を踏む。後から出るほうを握りつぶす */

// 押下時のOS設定で決める。Ctrl は関係しない
static bool android_blocks_mod(uint8_t mod) {
    if (!(mod & (MOD_MASK_GUI | MOD_MASK_ALT))) return false;
    const uint8_t other = (mod & MOD_MASK_GUI) ? MOD_MASK_ALT : MOD_MASK_GUI;
    return is_android() && (get_mods() & other);
}

/* 押下を握りつぶした修飾 (MOD_BIT)。先に押したほうを離しても出さない。出すと押した
 * 覚えの無い修飾が遅れて効く */
static uint8_t android_blocked_mods = 0;

// 英数レイヤーの KC_LGUI / KC_LALT。false なら消費済み
static bool process_android_gui_alt(uint16_t keycode, keyrecord_t *record) {
    const uint8_t mod = (keycode == KC_LGUI) ? MOD_BIT(KC_LEFT_GUI) : MOD_BIT(KC_LEFT_ALT);

    if (!record->event.pressed) {
        // 押している間に設定を変えても、離すぶんは消費する
        if (!(android_blocked_mods & mod)) return true;
        android_blocked_mods &= ~mod;
        return false;
    }
    if (!android_blocks_mod(mod)) return true;

    android_blocked_mods |= mod;
    return false;
}

/* 「Win を単独で押して離した」と見なされないよう、何も起こさないキーを空打ちする。
 * 右Ctrlはこのキーマップに無く、Win との組み合わせにショートカットも無い
 * (QMK の DUMMY_MOD_NEUTRALIZER_KEYCODE の推奨と同じ) */
static void neutralize_gui_tap(void) {
    tap_code(KC_RIGHT_CTRL);
}

// Win+. を Alt+. に読み替えて送ったあとの、ピリオドの解放待ち
static bool android_gui_dot_sent = false;

/* Win+. は、Windows の Win+. (絵文字) に揃えて Alt+. として送る。Win を重ねたままだと
 * Caps Lock、ただ離すとアプリ一覧になるので、離す前と押し直した後に空打ちを挟む。
 * Win を外している間にスキャンを挟まないよう、押下の中で送り切る。false なら消費済み */
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
 * MO() に任せると、先に離したほうがまだ押している側のぶんまで落とす (set_hold_layer と
 * 同じ理由) ので、ホールド中のFnを左右1ビットずつ覚え、Fnレイヤーと設定レイヤーの
 * 上げ下げを両方ここで持つ。
 *
 * 設定レイヤーの Fn の位置は透過にしておくこと。片方を押し直したとき、下のレイヤーの
 * Fn に落ちないと Fn として押せない */
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

// 英数レイヤーの MO(LAYER_FN)。左右は matrix の列で見分ける (windmill.h 参照)
static bool process_fn(keyrecord_t *record) {
    set_fn(record->event.key.col == FN_L_COL ? FN_BIT_L : FN_BIT_R, record->event.pressed);
    return false;
}

// 記号レイヤー (ひ / も)。Fnと同じく左右1ビットずつ覚える
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

/* QMK の tap-hold はキーごとに独立して決まり、親指の組のスペースや、未判定のキーを
 * 複数抱えたまま押した順に出す規則を表せないので、判定を自前で持つ (issue #85)。
 * 同じ段の Ctrl (MY_LCTL) もここに載せる (issue #95)。Ctrl は英数レイヤーでも透過で
 * 同じキーになるので、英数ベースでもこの判定を通る。
 *
 * - 押してから BOTTOM_HOLD_MS たてばホールド
 * - BOTTOM_INTERRUPT_MS 以上たっていて、後から最下段以外のキーが押されればホールド
 * - 後から押した最下段のキーがタップに決まった時点で、BOTTOM_INTERRUPT_MS 以上たって
 *   いればホールド (decide_bottom_tap)
 * - 未判定のまま離せばタップ。境界はどれも「以上」
 *
 * 未判定のキーより後のイベントは、押した順を保つためキューで待たせ、取り出した時点の
 * レイヤーと修飾で解決し直す。Shift 面の最下段 (KB_XTSU など) は同じ位置のかなレイヤーの
 * キーと役割が同じなので、状態はかなレイヤーのキーの番号で持つ (bottom_index) */
#define BOTTOM_INTERRUPT_MS 150 // これ以上たっていれば、別のキーの押下でホールドになる
#define BOTTOM_HOLD_MS      250 // 押したままこれだけたてばホールドになる

#define BOTTOM_COUNT (KB_NE - MY_LCTL + 1)
#define BOTTOM(keycode) ((keycode) - MY_LCTL)
#define BOTTOM_KEYCODE(i) (MY_LCTL + (i))
#define NO_BOTTOM 0xFF

static bool is_bottom_keycode(uint16_t keycode) {
    return keycode >= KEY_BOTTOM_FIRST && keycode <= KEY_BOTTOM_LAST;
}

/* Shift 面の 空 は こ と み の両方の位置にあるので、キーコードだけでは決まらない。
 * 最下段のキーでなければ NO_BOTTOM */
static uint8_t bottom_index(uint16_t keycode, keypos_t pos) {
    if (!is_bottom_keycode(keycode)) return NO_BOTTOM;
    if (keycode > KB_NE) keycode = keymap_key_to_keycode(LAYER_KANA, pos);
    return keycode >= MY_LCTL && keycode <= KB_NE ? BOTTOM(keycode) : NO_BOTTOM;
}

// ホールドの役割
enum {
    BOTTOM_HOLD_MOD,    // Ctrl / Win / Alt。ホールド中は英数レイヤー
    BOTTOM_HOLD_FN,     // Fn (set_fn)
    BOTTOM_HOLD_SYM,    // 記号レイヤー (set_sym)
    BOTTOM_HOLD_SHIFT,  // Shift。ホールド中は Shift 面
};

typedef struct {
    uint8_t hold;    // BOTTOM_HOLD_*
    uint8_t arg;     // 修飾なら MOD_BIT、Fn / 記号なら左右のビット
    uint8_t partner; // 同じ役割の相方。NO_BOTTOM なら無し
} bottom_key_t;

static const bottom_key_t PROGMEM bottom_keys[] = {
    [BOTTOM(MY_LCTL)] = {BOTTOM_HOLD_MOD, MOD_BIT(KC_LEFT_CTRL), NO_BOTTOM},
    [BOTTOM(KB_TSU)]  = {BOTTOM_HOLD_MOD, MOD_BIT(KC_LEFT_GUI), NO_BOTTOM},
    [BOTTOM(KB_SA)]   = {BOTTOM_HOLD_MOD, MOD_BIT(KC_LEFT_ALT), NO_BOTTOM},
    [BOTTOM(KB_SO)]   = {BOTTOM_HOLD_FN, FN_BIT_L, BOTTOM(KB_NE)},
    [BOTTOM(KB_HI)]   = {BOTTOM_HOLD_SYM, 1, BOTTOM(KB_MO)},
    [BOTTOM(KB_KO)]   = {BOTTOM_HOLD_SHIFT, MOD_BIT(KC_LEFT_SHIFT), BOTTOM(KB_MI)},
    [BOTTOM(KB_MI)]   = {BOTTOM_HOLD_SHIFT, MOD_BIT(KC_RIGHT_SHIFT), BOTTOM(KB_KO)},
    [BOTTOM(KB_MO)]   = {BOTTOM_HOLD_SYM, 2, BOTTOM(KB_HI)},
    [BOTTOM(KB_NE)]   = {BOTTOM_HOLD_FN, FN_BIT_R, BOTTOM(KB_SO)},
};
_Static_assert(ARRAY_SIZE(bottom_keys) == BOTTOM_COUNT, "bottom_keys[] の行が最下段のキーの数と合っていない");

static bottom_key_t get_bottom_key(uint8_t i) {
    bottom_key_t key;
    memcpy_P(&key, &bottom_keys[i], sizeof(key));
    return key;
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
static uint16_t       bottom_held  = 0;         // ホールドを出しているキー (1 << 番号)
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

/* 待たせるのは未判定のキーを押してから BOTTOM_HOLD_MS の間のイベントだけなので、
 * 両手で打っても収まる。溢れたら先頭を決めて空ける */
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

// もう一度 QMK に処理させる。キーコードはその時点のレイヤーで引き直される
static void bottom_replay(keyrecord_t *record) {
    const bool replaying = bottom_replaying;
    bottom_replaying     = true;
    process_record(record);
    bottom_replaying = replaying;
}

/* 未判定のキーをタップに決め、先に押されて BOTTOM_INTERRUPT_MS 以上たった未判定の
 * キーをホールドにする。後のキーはそのホールドのレイヤーで出る (ひ を押しながら こ を
 * タップすると、記号レイヤーの「|」) */
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
 * - Y の押下から解放までが BOTTOM_HOLD_MS 未満
 * - Y の解放の時点で、X と Y のどちらも他のキーにシフトとしてかかっていない
 *
 * スペースは、Y の解放と、X が BOTTOM_INTERRUPT_MS に達した時点 (先に離したなら X の
 * 解放) の遅い方で決まり、後から X がかかっても取り消さない。そのとき X を押していれば
 * X はホールドになり、続けて打つキーに Shift として乗る。
 *
 * 満たさなくなった時点で組は解け、それぞれ通常の判定に戻る (thumb_pair_fail)。
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
        case BOTTOM_HOLD_MOD:
            if (android_blocks_mod(key.arg)) return;
            register_mods(key.arg);
            set_hold_layer(get_mods());
            break;
        case BOTTOM_HOLD_FN:
            set_fn(key.arg, true);
            break;
        case BOTTOM_HOLD_SYM:
            set_sym(key.arg, true);
            break;
        case BOTTOM_HOLD_SHIFT:
            register_mods(key.arg);
            layer_on(LAYER_KANA_SHIFT);
            break;
    }
    bottom_held |= 1 << i;
}

// Shift 面を上げている (こ / み のどちらかがホールド中)
#define BOTTOM_SHIFT_HELD ((1 << BOTTOM(KB_KO)) | (1 << BOTTOM(KB_MI)))

static void release_bottom_hold(uint8_t i) {
    if (!(bottom_held & (1 << i))) return; // タップだった、または握りつぶした
    bottom_held &= ~(1 << i);

    const bottom_key_t key = get_bottom_key(i);
    switch (key.hold) {
        case BOTTOM_HOLD_MOD:
            unregister_mods(key.arg);
            set_hold_layer(get_mods());
            break;
        case BOTTOM_HOLD_FN:
            set_fn(key.arg, false);
            break;
        case BOTTOM_HOLD_SYM:
            set_sym(key.arg, false);
            break;
        case BOTTOM_HOLD_SHIFT:
            unregister_mods(key.arg);
            if (!(bottom_held & BOTTOM_SHIFT_HELD)) layer_off(LAYER_KANA_SHIFT);
            break;
    }
}

/* タップを、出す時点のレイヤーで引き直して出す。記号レイヤーなど自分以外のキーに
 * 引かれたら、押下と解放を作って QMK に処理させ、通常のキーと同じ扱いにする
 * (Fn を押しながらの Ctrl のタップは、Fnレイヤーの MY_IME になる)。
 * 空 は親指の組が成り立ったときのものなので、組にならないタップは こ / み として出す */
static void send_bottom_tap(uint8_t i, keypos_t pos) {
    uint16_t keycode = keymap_key_to_keycode(layer_switch_get_layer(pos), pos);
    if (bottom_index(keycode, pos) != i) {
        keyrecord_t record = {.event = MAKE_KEYEVENT(pos.row, pos.col, true)};
        bottom_replay(&record);
        record.event = (keyevent_t)MAKE_KEYEVENT(pos.row, pos.col, false);
        bottom_replay(&record);
        return;
    }

    if (keycode == MY_LCTL) {
        switch_lang();
        return;
    }
    if (keycode == KB_SPC) keycode = BOTTOM_KEYCODE(i);
    send_output(get_key_output(keycode), is_alpha_output(keycode));
}

/* 親指の組の半角スペース。組のキーはどちらも離れているので、ホールドの Shift はここで
 * 下ろす (後から届く解放では何もしない)。下ろした Shift のレポートとスペースの間にも
 * ウェイトを挟む (IME_WAIT_MS と同じ理由) */
static void send_thumb_space(void) {
    const bool had_shift = get_mods() & MOD_MASK_SHIFT;
    if (!bottom_states[BOTTOM(KB_KO)].pressed) release_bottom_hold(BOTTOM(KB_KO));
    if (!bottom_states[BOTTOM(KB_MI)].pressed) release_bottom_hold(BOTTOM(KB_MI));

    if (had_shift && !(get_mods() & MOD_MASK_SHIFT)) wait_ms(IME_WAIT_MS);
    send_output(get_key_output(KB_SPC), false);
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
            if (age < 0) continue; // このキーより前の押下 (QMKの tap-hold で遅れて届いた、英数レイヤーの LSFT_T(KC_SPC) など)
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
            const uint8_t i = bottom_index(keycode, record->event.key);
            if (i != NO_BOTTOM) bottom_press(i, record); // NO_BOTTOM はキーマップの誤り。捨てる
        } else {
            bottom_interrupt(now);
            consumed = false;
        }
    } else {
        /* 解放は位置で見分ける。タップを記号レイヤーで出したあとなどは、QMKが覚えている
         * 押下時のレイヤーが変わっていて、キーコードが別物で届く */
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
    // 待たせたイベントは後でここへ流し直すので、そのときは通さない
    if (!bottom_replaying && !process_bottom_row(keycode, record)) return false;
    if (is_bottom_keycode(keycode)) return false; // 流し直しで最下段のキーに引かれた。押下を見ていないので捨てる

    if (!process_record_user(keycode, record)) return false;

    if (keycode >= KEY_CHAR_FIRST && keycode <= KEY_CHAR_LAST) { // 数字・記号・かな
        return process_char_key(keycode, record);
    }

    if (!process_jis_alpha_symbol(keycode, record)) return false; // 英数レイヤーの ; ' \ (JIS のときだけ)

    switch (keycode) {
#ifdef WINDMILL_LED_ENABLE
        case MY_DARK: // LEDの明るさ 強/弱
            if (record->event.pressed) {
                toggle_darkmode();
            }
            return false;
#endif

        case MY_WIN:  if (record->event.pressed) set_host_setting(HOST_OS, OS_WIN); return false;
        case MY_ANDR: if (record->event.pressed) set_host_setting(HOST_OS, OS_ANDR); return false;
        case MY_JIS:  if (record->event.pressed) set_host_setting(HOST_LAYOUT, WINDMILL_LAYOUT_JIS); return false;
        case MY_US:   if (record->event.pressed) set_host_setting(HOST_LAYOUT, WINDMILL_LAYOUT_US); return false;

        /* Fn+Ctrl: IMEだけを今のベースレイヤーの側へ切り替える。Android のトグルが
         * ずれたとき、キーボードを動かさずに合わせ直すため。Windows では揃っていれば
         * 何も起きないが、Android では揃っている状態で押すと逆にずれる */
        case MY_IME:
            if (record->event.pressed) {
                send_lang_to_host(get_highest_layer(default_layer_state));
            }
            return false;

        case KC_LGUI: // 英数レイヤーの Win / Alt (Ctrlのホールドで英数レイヤーが上がったときを含む)
        case KC_LALT:
            // 先に見る。握りつぶした修飾は押されていないので、レイヤーにも効かせない
            if (!process_android_gui_alt(keycode, record)) {
                return false;
            }
            /* 修飾そのものはこの後QMKが押し離しするので、その後の修飾で決めておく。
             * Ctrl を先に離しても、Win / Alt を押している間は英数レイヤーのまま */
            set_hold_layer(record->event.pressed ? get_mods() | MOD_BIT(keycode) : get_mods() & ~MOD_BIT(keycode));
            break;

        case ALPHA_FN: // 英数レイヤーの Fn
            return process_fn(record);

        case KC_DOT: // 英数レイヤーのピリオド
            if (!process_android_gui_dot(record)) {
                return false;
            }
            break;
    }

    return windmill_board_process_record(keycode, record);
}

void matrix_scan_kb(void) {
#ifdef WINDMILL_LED_ENABLE
    update_led_timeout();
#endif

    bottom_tick(timer_read());

    matrix_scan_user();
}

void keyboard_post_init_kb(void) {
    reset_default_layer();

    windmill_config.raw = eeconfig_read_kb();
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
