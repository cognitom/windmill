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
 * default_layer_set を使う。かな入力はOS側のIMEに任せる (レイヤー0が素の
 * QWERTY+数字段で、標準のJISかな配置になる)。
 *
 * LEDはキーコードの種類ごとに色を割り当てる独自実装で、こちらは従来のまま。 */

#include "windmill.h"

#define KEY_COUNT (MATRIX_ROWS * MATRIX_COLS)

typedef union {
    uint32_t raw;
    struct {
        bool is_android : 1;   // MY_O/MY_P のShift時出力をAndroid向けにする
        bool led_darkmode : 1; // LEDを暗いほうの配色にする
    };
} windmill_config_t;
static windmill_config_t windmill_config;

/*
 * RGB Matrix / Light
 */

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
                    (keycode == KC_TRANSPARENT) ? CL_TRANS : windmill_process_keycolor_user(windmill_base_keycode(keycode));
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

/*
 * MY_LCTL: tap = 英数, double-tap = かな, hold = Ctrl + 英数レイヤー
 *
 * 別キー割り込みで tapping term を待たず即ホールド確定する。
 * tap/double-tap 時にIMEと揃えてベースレイヤーも切り替える。
 */

#define TD_TAP_KC       KC_LNG2     // 1回タップ: 英数
#define TD_DOUBLE_KC    KC_LNG1     // 2回タップ: かな
#define TD_HOLD_MOD     KC_LCTL     // ホールド時の装飾
#define TD_LAYER        LAYER_ALPHA // ホールド時のレイヤー。かな中でもCtrl+Cなどが英字で打てる
#define TD_TAP_LAYER    LAYER_ALPHA // tap(英数)時のベースレイヤー
#define TD_DOUBLE_LAYER LAYER_KANA  // double-tap(かな)時のベースレイヤー

#define TD_TERM      TAPPING_TERM
#define TD_DTAP_TERM 180

typedef enum {
    TD_IDLE,
    TD_PRESSED,   // 押下中・未確定
    TD_WAIT_DTAP, // 1タップ後、2回目待ち
} td_phase_t;

static td_phase_t td_phase       = TD_IDLE;
static uint16_t   td_timer       = 0;
static bool       td_hold_active = false;

static void td_hold_on(void) {
    register_mods(MOD_BIT(TD_HOLD_MOD));
    layer_on(TD_LAYER);
    td_hold_active = true;
}

static void td_hold_off(void) {
    layer_off(TD_LAYER);
    unregister_mods(MOD_BIT(TD_HOLD_MOD));
    td_hold_active = false;
}

/* tap/double-tap 確定。KC_LNGx と同時にベースレイヤーもIMEに揃える。
 * default_layer_set はEEPROMを書かないので頻繁な切り替えでも安全 */
static void td_tap_confirm(void) {
    tap_code16(TD_TAP_KC);
    default_layer_set((layer_state_t)1 << TD_TAP_LAYER);
}

static void td_double_confirm(void) {
    tap_code16(TD_DOUBLE_KC);
    default_layer_set((layer_state_t)1 << TD_DOUBLE_LAYER);
}

/*
 * Shift時に別の記号を出すキー
 */

/* Shift付きで押されたらShiftを一時抑制して shifted を、
 * そうでなければ plain を送出する (QMKのkey override相当) */
static bool process_shift_pair(uint16_t plain, uint16_t shifted, keyrecord_t *record) {
    if (record->event.pressed) {
        const uint8_t mods = get_mods();
        if (mods & MOD_MASK_SHIFT) {
            del_mods(MOD_MASK_SHIFT);
            tap_code16(shifted);
            set_mods(mods);
        } else {
            tap_code16(plain);
        }
    }
    return false;
}

// MY_W〜MY_A の {Shiftなし, Shiftあり} 出力表。windmill.h のenum順と一致させること
static const uint16_t my_shift_pairs[][2] = {
    [MY_W - MY_W]    = {KC_W, S(KC_EQL)},     // +
    [MY_R - MY_W]    = {KC_R, KC_BSLS},       // バックスラッシュ
    [MY_U - MY_W]    = {KC_U, KC_MINS},       // -
    [MY_LBRC - MY_W] = {KC_LBRC, KC_RBRC},    // ]
    [MY_K - MY_W]    = {KC_K, S(KC_COMM)},    // <
    [MY_L - MY_W]    = {KC_L, S(KC_DOT)},     // >
    [MY_SCLN - MY_W] = {KC_SCLN, S(KC_SLSH)}, // ?
    [MY_QUOT - MY_W] = {KC_QUOT, S(KC_MINS)}, // _
    [MY_A - MY_W]    = {KC_A, S(KC_Z)},       // Z
};

/*
 * 親指Shift
 */

/* tap = B/N, hold = Shift (QMKのtap-holdのまま),
 * tap with Shift = 半角スペース (Shift抑制)。ただしShift開始後に
 * 他のキーを押していたら誤入力ガードとして反応しない */

// Shift(親指hold)開始後に他のキーが押されたらダーティ
static bool thumb_shift_dirty = false;

// falseを返したらそのイベントは消費済み(以降の処理をスキップ)
static bool process_thumb_shift(keyrecord_t *record) {
    if (record->tap.count && record->event.pressed) { // tap確定
        const uint8_t mods = get_mods();
        if (mods & MOD_MASK_SHIFT) {
            if (!thumb_shift_dirty) {
                del_mods(MOD_MASK_SHIFT);
                tap_code16(KC_SPC);
                set_mods(mods);
            }
            return false; // ダーティ時は何も出さない
        }
    } else if (!record->tap.count && record->event.pressed) { // hold確定 = Shift開始
        thumb_shift_dirty = false;
    }
    return true; // 通常のtap(B/N)とholdはQMKに任せる
}

/*
 * かなレイヤー上での記号入力
 */

/* MY_O/MY_PのShift時出力 (「」) はOSのIME実装で必要なキーが異なるため、
 * MY_WIN / MY_ANDR で切り替える。設定はEEPROMに永続化し、挿し直しても保持する。
 * EEPROMリセット時はWindows/デスクトップ向け(false)に戻る */
static bool is_android = false;

static void set_is_android(bool val) {
    if (is_android == val) return; // 無変更ならEEPROMを書かない
    is_android                = val;
    windmill_config.is_android = val;
    eeconfig_update_kb(windmill_config.raw);
}

// IME切り替え待ち。効かない場合は増やし、もたつくなら減らす
#define LT2_IME_WAIT_MS 30

/* 記号レイヤーの数字・記号キーか (レイヤー配置と一致させること)。
 * 矢印キーとKC_TRNSで下位レイヤーに落ちるキーは含めない */
static bool is_sym_ime_wrap_target(uint16_t keycode) {
    switch (keycode) {
        case KC_1 ... KC_0:
        case S(KC_1) ... S(KC_0):
        case KC_GRV:
        case S(KC_GRV):
        case KC_EQL:
        case S(KC_EQL):
        case KC_MINS:
        case S(KC_MINS):
        case KC_LBRC:
        case S(KC_LBRC):
        case KC_RBRC:
        case S(KC_RBRC):
        case S(KC_BSLS):
        case S(KC_SLSH):
            return true;
    }
    return false;
}

/*
 * QMK callbacks
 */

// キーコード確定「前」に呼ばれるため、ここで layer_on すれば
// 割り込みキーがレイヤー1 + Ctrl で解決される
bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (keycode != MY_LCTL && record->event.pressed) {
        if (td_phase == TD_PRESSED && !td_hold_active) {
            td_hold_on(); // 割り込み → 即ホールド確定
        } else if (td_phase == TD_WAIT_DTAP) {
            td_phase = TD_IDLE;
            td_tap_confirm(); // 保留中のタップを先にflush
        }
    }
    return pre_process_record_user(keycode, record);
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_led_timeout(keycode, record)) return false;
    if (!process_record_user(keycode, record)) return false;

    // 親指Shift以外のキー押下でダーティ化
    if (record->event.pressed && keycode != THUMB_SHIFT_B && keycode != THUMB_SHIFT_N) {
        thumb_shift_dirty = true;
    }

    /* ベースがかなのとき、記号レイヤーの数字・記号は
     * 英数(LNG2)に切り替えて送出し、かな(LNG1)に戻す */
    if (layer_state_is(LAYER_SYM) && get_highest_layer(default_layer_state) == LAYER_KANA && is_sym_ime_wrap_target(keycode)) {
        if (record->event.pressed) {
            // IMEのモード切り替えは非同期なので、間にウェイトを挟まないと
            // 切り替え完了前にキーが届いて全角のまま入力される
            tap_code16(KC_LNG2);
            wait_ms(LT2_IME_WAIT_MS);
            tap_code16(keycode);
            wait_ms(LT2_IME_WAIT_MS);
            tap_code16(KC_LNG1);
        }
        return false; // releaseも消費 (未registerのunregisterを防ぐ)
    }

    switch (keycode) {
        case MY_DARK: // LEDの明るさ 強/弱
            if (record->event.pressed) {
                toggle_darkmode();
            }
            return false;

        case MY_WIN:
            if (record->event.pressed) {
                set_is_android(false);
            }
            return false;

        case MY_ANDR:
            if (record->event.pressed) {
                set_is_android(true);
            }
            return false;

        case MY_O: // O / 「 (OS依存)
            return process_shift_pair(KC_O, is_android ? S(KC_RBRC) : KC_LCBR, record);

        case MY_P: // P / 」 (OS依存)
            return process_shift_pair(KC_P, is_android ? S(KC_BSLS) : KC_RCBR, record);

        case MY_W ... MY_A: // Shiftで別の記号を出すキー
            return process_shift_pair(my_shift_pairs[keycode - MY_W][0], my_shift_pairs[keycode - MY_W][1], record);

        case THUMB_SHIFT_B: // 親指Shift
        case THUMB_SHIFT_N:
            if (!process_thumb_shift(record)) {
                return false;
            }
            break; // 通常処理へ (tap=B/N, hold=Shift)

        case MY_LCTL:
            if (record->event.pressed) {
                if (td_phase == TD_WAIT_DTAP) {
                    td_phase = TD_IDLE;
                    td_double_confirm(); // ダブルタップ確定
                } else {
                    td_phase = TD_PRESSED;
                    td_timer = timer_read();
                }
            } else {
                if (td_phase == TD_PRESSED) {
                    if (td_hold_active) {
                        td_hold_off();
                        td_phase = TD_IDLE;
                    } else {
                        td_phase = TD_WAIT_DTAP;
                        td_timer = timer_read();
                    }
                }
            }
            return false;
    }

    return true;
}

void matrix_scan_kb(void) {
    update_led_timeout();

    if (td_phase == TD_PRESSED && !td_hold_active && timer_elapsed(td_timer) > TD_TERM) {
        td_hold_on();
    }
    if (td_phase == TD_WAIT_DTAP && timer_elapsed(td_timer) > TD_DTAP_TERM) {
        td_phase = TD_IDLE;
        td_tap_confirm();
    }

    matrix_scan_user();
}

void keyboard_post_init_kb(void) {
    windmill_config.raw = eeconfig_read_kb();
    is_android          = windmill_config.is_android;
    led_darkmode        = windmill_config.led_darkmode;

    // keymap.c 側で windmill_init_keycolors() を呼ぶので、配色を組む前に済ませる
    keyboard_post_init_user();

#ifdef RGB_MATRIX_ENABLE
    rgb_matrix_mode_noeeprom(RGB_MATRIX_NONE); // アニメーションなし。全て自前で描く
    rgb_matrix_sethsv_noeeprom(HSV_OFF);
#elif defined(RGBLIGHT_ENABLE)
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_sethsv_noeeprom(HSV_OFF);
#endif

    cache_keycolors();
    led_initialized = true;
    refresh_keycolors(layer_state, default_layer_state);
}

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

#ifdef RGB_MATRIX_ENABLE
bool rgb_matrix_indicators_kb(void) {
    if (!rgb_matrix_indicators_user()) return false;
    if (!led_on || !led_initialized) return true;

    apply_keycolors();
    return true;
}
#endif
