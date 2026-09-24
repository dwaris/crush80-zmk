#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include "overlay.h"
#include "color.h"
#include "led_map.h"     /* rrgb_led_for_position */
#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/ble.h>
#endif
#include <zmk/endpoints.h>
#include "overlay.h"

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
static volatile uint8_t  s_battery;
static volatile uint32_t s_bat_until;

void rrgb_overlay_set_caps(bool on)        { s_caps = on; }
void rrgb_overlay_set_fn(bool active)      { s_fn = active; }
void rrgb_overlay_set_battery(uint8_t pct) { s_battery = pct; }
void rrgb_overlay_battery_show(uint32_t tick) { s_bat_until = tick + BAT_SHOW_FRAMES; }

bool rrgb_overlay_active(uint32_t tick) {
    return s_caps || s_fn || (tick < s_bat_until);
}

static void set_pos(struct rrgb *px, uint16_t n, uint8_t pos, struct rrgb c) {
    int led = rrgb_led_for_position(pos);
    if (led >= 0 && led < (int)n) { px[led] = c; }
}

void rrgb_overlay_render(struct rrgb *px, uint16_t n, uint32_t tick) {
    /* 1. Fn overlay: light bound keys white, with color-coded functional groups.
     *    Other keys remain untouched to keep background RGB effects live. */
    if (s_fn) {
        /* Default bound keys: White (Main backlight, F-row, navigation shortcuts) */
        for (unsigned k = 0; k < FN_KEYS_COUNT; k++) {
            set_pos(px, n, fn_keys[k], (struct rrgb){255, 255, 255});
        }

        /* Side lightbar controls: Cyan / Aqua (Key '/?' pos 74, Key ';' pos 61, Key ''' pos 62) */
        set_pos(px, n, 74, (struct rrgb){0, 220, 255});
        set_pos(px, n, 61, (struct rrgb){0, 220, 255});
        set_pos(px, n, 62, (struct rrgb){0, 220, 255});

        /* Logo badge controls: Amber / Orange (Key 'P' pos 44, Key '[' pos 45, Key ']' pos 46) */
        set_pos(px, n, 44, (struct rrgb){255, 120, 0});
        set_pos(px, n, 45, (struct rrgb){255, 120, 0});
        set_pos(px, n, 46, (struct rrgb){255, 120, 0});

#if IS_ENABLED(CONFIG_ZMK_BLE)
        /* Highlight active connection mode: USB (Key 5) or active BT profile (Key 1..3) */
        if (zmk_endpoint_get_preferred_transport() == ZMK_TRANSPORT_USB) {
            set_pos(px, n, 22, (struct rrgb){0, 255, 128}); /* Key '5' (USB): Cyan */
        } else {
            int prof = zmk_ble_active_profile_index();
            if (prof >= 0 && prof < 3) {
                struct rrgb bt_col = zmk_ble_active_profile_is_connected()
                    ? (struct rrgb){0, 255, 0}    /* Green: Connected */
                    : (struct rrgb){0, 100, 255}; /* Blue: Searching / Pairing */
                set_pos(px, n, (uint8_t)(18 + prof), bt_col); /* Key '1', '2', or '3' */
            }
        }
#endif
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
