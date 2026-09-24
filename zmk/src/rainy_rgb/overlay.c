#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include "overlay.h"
#include "color.h"
#include "led_map.h"     /* rrgb_led_for_position */

#define BAT_SHOW_FRAMES  150  /* ~3s at 50fps */
#define BAT_SEG_FIRST    18   /* number row keys 1..0 = positions 18..27 */
#define BAT_SEG_COUNT    10

/* Fn-active keymap positions on Crush 80 */
static const uint8_t fn_keys[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, /* Esc, F1-F12, F13 (Mute) */
    18, 19, 20, 22,                                 /* Key 1..3 (BT_SEL 0..2), Key 5 (OUT_USB) */
    30, 31,                                         /* Backspace (RGB_TOG), Insert (BT_CLR) */
    34,                                             /* Tab (OUT_TOG) */
    44, 45, 46, 47,                                 /* P (RGB_LOG), [, ], \ (LOM, LOC, EFF) */
    61, 62, 63,                                     /* ; (RGB_SIM), ' (RGB_SIC), Enter (HUI) */
    74,                                             /* /? (RGB_SID) */
    76,                                             /* Up (RGB_BRI) */
    80,                                             /* Space (RGB_BAT) */
    82,                                             /* Fn key itself */
    85, 86, 87,                                     /* Left (RGB_SPD), Down (RGB_BRD), Right (RGB_SPI) */
};
#define FN_KEYS_COUNT (sizeof(fn_keys) / sizeof(fn_keys[0]))

static volatile bool     s_caps;
static volatile bool     s_fn;
static volatile bool     s_fn_is_usb = true;
static volatile uint8_t  s_fn_bt_prof;
static volatile bool     s_fn_bt_connected;
static volatile uint8_t  s_battery;
static volatile uint32_t s_bat_until;

void rrgb_overlay_set_caps(bool on)        { s_caps = on; }
void rrgb_overlay_set_fn(bool active, bool is_usb, uint8_t bt_prof, bool bt_connected) {
    s_fn = active;
    s_fn_is_usb = is_usb;
    s_fn_bt_prof = bt_prof;
    s_fn_bt_connected = bt_connected;
}
void rrgb_overlay_set_battery(uint8_t pct) { s_battery = pct; }
void rrgb_overlay_battery_show(uint32_t tick) { s_bat_until = tick + BAT_SHOW_FRAMES; }

bool rrgb_overlay_active(uint32_t tick) {
    return s_caps || s_fn || (tick < s_bat_until);
}

static inline void set_pos(struct rrgb *px, uint16_t n, uint8_t pos, struct rrgb c) {
    int led = rrgb_led_for_position(pos);
    if (led >= 0 && led < (int)n) { px[led] = c; }
}

void rrgb_overlay_render(struct rrgb *px, uint16_t n, uint32_t tick) {
    /* 1. Fn overlay: highlight bound keys with color-coded functional groups.
     *    Non-bound keys remain untouched so background RGB animations keep playing. */
    if (s_fn) {
        /* Default bound keys: White (F-row media/function shortcuts, arrows) */
        for (unsigned k = 0; k < FN_KEYS_COUNT; k++) {
            set_pos(px, n, fn_keys[k], (struct rrgb){255, 255, 255});
        }

        /* Bootloader: Red (Esc pos 0) */
        set_pos(px, n, 0, (struct rrgb){255, 30, 30});

        /* Connectivity shortcuts:
         * - BT Profiles 1..3: Blue (Keys 1..3, pos 18..20)
         * - USB Output: Cyan (Key 5, pos 22)
         * - Output Toggle: Purple (Tab pos 34)
         * - BT Clear: Amber/Red (Insert pos 31) */
        set_pos(px, n, 18, (struct rrgb){0, 100, 255});
        set_pos(px, n, 19, (struct rrgb){0, 100, 255});
        set_pos(px, n, 20, (struct rrgb){0, 100, 255});
        set_pos(px, n, 22, (struct rrgb){0, 220, 255});
        set_pos(px, n, 34, (struct rrgb){180, 0, 255});
        set_pos(px, n, 31, (struct rrgb){255, 60, 0});

        /* Highlight active connection mode: USB (Key 5) or active BT profile (Key 1..3) */
        if (s_fn_is_usb) {
            set_pos(px, n, 22, (struct rrgb){0, 255, 60}); /* Key '5' (USB): Green */
        } else if (s_fn_bt_prof < 3) {
            struct rrgb bt_col = s_fn_bt_connected
                ? (struct rrgb){0, 255, 60}    /* Green: Connected */
                : (struct rrgb){0, 200, 255}; /* Cyan: Searching / Pairing */
            set_pos(px, n, (uint8_t)(18 + s_fn_bt_prof), bt_col); /* Key '1', '2', or '3' */
        }

        /* Main RGB controls:
         * - Toggle: Amber (Backspace pos 30)
         * - Effect: Magenta (Backslash pos 47)
         * - Hue: Yellow (Enter pos 63) */
        set_pos(px, n, 30, (struct rrgb){255, 150, 0});
        set_pos(px, n, 47, (struct rrgb){255, 0, 200});
        set_pos(px, n, 63, (struct rrgb){255, 220, 0});

        /* Side lightbar controls: Cyan / Aqua (Key '/?' pos 74, Key ';' pos 61, Key ''' pos 62) */
        set_pos(px, n, 74, (struct rrgb){0, 220, 255});
        set_pos(px, n, 61, (struct rrgb){0, 220, 255});
        set_pos(px, n, 62, (struct rrgb){0, 220, 255});

        /* Logo badge controls: Amber / Orange (Key 'P' pos 44, Key '[' pos 45, Key ']' pos 46) */
        set_pos(px, n, 44, (struct rrgb){255, 120, 0});
        set_pos(px, n, 45, (struct rrgb){255, 120, 0});
        set_pos(px, n, 46, (struct rrgb){255, 120, 0});

        /* Battery check: Green (Space pos 80) */
        set_pos(px, n, 80, (struct rrgb){0, 255, 60});

        /* Fn key itself: Soft Ice Blue (pos 82) */
        set_pos(px, n, 82, (struct rrgb){160, 200, 255});
    }
    /* 2. CapsLock: white on the logo LED(s). */
    if (s_caps) {
        for (uint16_t i = LOGO_LED_FIRST; i <= LOGO_LED_LAST && i < n; i++) {
            px[i] = (struct rrgb){255, 255, 255};
        }
    }
    /* 3. Battery gauge: 10-segment bar on the number row, ~3s window. */
    if (tick < s_bat_until) {
        uint8_t lit = (uint8_t)((s_battery * BAT_SEG_COUNT + 50) / 100);  /* 0..10 */
        uint8_t hue = (uint8_t)(85 * (uint16_t)s_battery / 100);          /* 0%=red,100%=green */
        for (uint8_t s = 0; s < BAT_SEG_COUNT; s++) {
            struct rrgb c = (s < lit) ? hsv2rgb(hue, 255, 255)
                                      : (struct rrgb){8, 8, 8};
            set_pos(px, n, (uint8_t)(BAT_SEG_FIRST + s), c);
        }
    }
}
