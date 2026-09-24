#include <stddef.h>
#include "led_map.h"

#define RRGB_N 109
#define KEY_COUNT 88

static const uint8_t pos_to_led[KEY_COUNT] = {
    /* Row 0: pos 0..16 (offset 0) */
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
    /* Row 1: pos 17..33 (offset 0) */
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
    /* Row 2: pos 34..50 (offset 0) */
    34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    /* Row 3: pos 51..63 (offset +1, pad 51 unpopulated) */
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
    /* Row 4: pos 64..76 (offset +2, pad 65 unpopulated) */
    66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
    /* Row 5: pos 77..87 (pads 82, 84 are unpopulated split space pads) */
    79, 80, 81, 83, 85, 86, 87, 88, 89, 90, 91,
};

static const struct led_xy led_positions[RRGB_N] = {
    /* Row 0: LEDs 0..16 (17 keys: Esc to Pause) */
    {  7,  7}, { 25,  7}, { 40,  7}, { 54,  7}, { 68,  7}, { 86,  7},
    {101,  7}, {115,  7}, {129,  7}, {147,  7}, {162,  7}, {176,  7},
    {190,  7}, {205,  7}, {226,  7}, {241,  7}, {255,  7},
    /* Row 1: LEDs 17..33 (17 keys: ` to PgUp) */
    {  7, 25}, { 22, 25}, { 36, 25}, { 50, 25}, { 65, 25}, { 79, 25},
    { 93, 25}, {108, 25}, {122, 25}, {136, 25}, {151, 25}, {165, 25},
    {180, 25}, {201, 25}, {226, 25}, {241, 25}, {255, 25},
    /* Row 2: LEDs 34..50 (17 keys: Tab to PgDn) */
    { 11, 40}, { 29, 40}, { 43, 40}, { 57, 40}, { 72, 40}, { 86, 40},
    {101, 40}, {115, 40}, {129, 40}, {144, 40}, {158, 40}, {172, 40},
    {187, 40}, {205, 40}, {226, 40}, {241, 40}, {255, 40},
    /* Pad 51: unpopulated ISO Enter pad */
    {215, 47},
    /* Row 3: LEDs 52..64 (13 keys: Caps to Enter) */
    { 13, 54}, { 32, 54}, { 47, 54}, { 61, 54}, { 75, 54}, { 90, 54},
    {104, 54}, {119, 54}, {133, 54}, {147, 54}, {162, 54}, {176, 54},
    {199, 54},
    /* Pad 65: unpopulated ISO NUBS pad */
    { 22, 68},
    /* Row 4: LEDs 66..78 (13 keys: LShift to Up) */
    { 16, 68}, { 40, 68}, { 54, 68}, { 68, 68}, { 83, 68}, { 97, 68},
    {111, 68}, {126, 68}, {140, 68}, {154, 68}, {169, 68}, {196, 68},
    {241, 68},
    /* Row 5: LEDs 79..91 (11 keys + 2 unpopulated split space pads 82, 84) */
    {  9, 83}, { 27, 83}, { 45, 83}, { 72, 83}, { 99, 83}, {126, 83},
    {153, 83}, {171, 83}, {189, 83}, {207, 83}, {226, 83}, {241, 83},
    {255, 83},
    /* LEDs 92..97: Left Side Lightbar (6 LEDs) */
    {  0, 10}, {  0, 22}, {  0, 35}, {  0, 48}, {  0, 61}, {  0, 75},
    /* LEDs 98..103: Right Side Lightbar (6 LEDs) */
    {255, 10}, {255, 22}, {255, 35}, {255, 48}, {255, 61}, {255, 75},
    /* LEDs 104..108: Logo LEDs (5 LEDs) */
    {236, 54}, {239, 54}, {241, 54}, {244, 54}, {246, 54},
};

const struct led_xy *rrgb_led_xy = led_positions;

int rrgb_led_for_position(uint32_t position) {
    if (position >= KEY_COUNT) {
        return -1;
    }
    return pos_to_led[position];
}

const struct led_xy *rrgb_xy_for_position(uint32_t position) {
    int led = rrgb_led_for_position(position);
    if (led < 0 || led >= RRGB_N) {
        return NULL;
    }
    return &rrgb_led_xy[led];
}
