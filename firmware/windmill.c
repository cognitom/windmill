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

typedef union {
    uint32_t raw;
    struct {
        bool is_android : 1;   // MY_O/MY_P のShift時出力をAndroid向けにする
        bool led_darkmode : 1; // LEDを暗いほうの配色にする (LED非搭載機では未使用)
    };
} windmill_config_t;
static windmill_config_t windmill_config;

/*
 * 起動時のベースレイヤー
 */

/* 英数/かなの切り替え (MY_LCTL) はEEPROMに保存しない (td_tap_confirm 参照)。
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

/* Shiftとしてホールド中の親指Shift。左右で1ビットずつ持つ。
 *
 * 左右とも左Shiftになった (issue #37) ので、QMKの修飾ビットは両者で共有される。
 * 参照カウントが無いため、素のままだと先に離したほうの unregister_mods() で
 * Shiftが落ち、もう片方を押していてもホストからはShiftが離れて見える。
 * 押しっぱなしのほうが残っているうちは離すイベントごと消費して、
 * 持ち替え (ハンドオーバー) を途切れさせない。 */
static uint8_t thumb_shift_held = 0;

#define THUMB_SHIFT_BIT(keycode) ((keycode) == THUMB_SHIFT_B ? 1 : 2)

// falseを返したらそのイベントは消費済み(以降の処理をスキップ)
static bool process_thumb_shift(uint16_t keycode, keyrecord_t *record) {
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
        thumb_shift_held |= THUMB_SHIFT_BIT(keycode);
    } else if (!record->event.pressed && !record->tap.count) { // hold解放
        thumb_shift_held &= ~THUMB_SHIFT_BIT(keycode);
        // もう片方がまだShiftとして押されているなら、QMKに離させない
        if (thumb_shift_held) return false;
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

/* LT(2,KC_M) (も) をShiftを押しながらタップしたら、半角「?」を送る。かな入力の
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
 * ホールド (Symレイヤーへの遷移) はQMKの通常のLT()処理に任せる */
static bool process_kana_qmark(keyrecord_t *record) {
    if (record->tap.count && record->event.pressed) { // タップ確定
        const uint8_t shift = get_mods() & MOD_MASK_SHIFT;
        if (shift) {
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
            return false;
        }
    }
    return true; // Shiftなしのタップ、ホールドはQMKに任せる
}

/*
 * QMK callbacks
 */

// キーコード確定「前」に呼ばれるため、ここで layer_on すれば
// 割り込みキーがレイヤー1 + Ctrl で解決される
bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    windmill_board_pre_process_record(keycode, record);

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
#ifdef WINDMILL_LED_ENABLE
    if (!process_led_timeout(keycode, record)) return false;
#endif
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
            wait_ms(IME_WAIT_MS);
            tap_code16(keycode);
            wait_ms(IME_WAIT_MS);
            tap_code16(KC_LNG1);
        }
        return false; // releaseも消費 (未registerのunregisterを防ぐ)
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

        case KANA_QMARK_KEY: // も。Shift+タップで半角「?」
            if (!process_kana_qmark(record)) {
                return false;
            }
            break; // 通常のtap(も)とhold(Symレイヤー)はQMKに任せる

        case THUMB_SHIFT_B: // 親指Shift
        case THUMB_SHIFT_N:
            if (!process_thumb_shift(keycode, record)) {
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

    return windmill_board_process_record(keycode, record);
}

void matrix_scan_kb(void) {
#ifdef WINDMILL_LED_ENABLE
    update_led_timeout();
#endif

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
    reset_default_layer();

    windmill_config.raw = eeconfig_read_kb();
    is_android          = windmill_config.is_android;
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
