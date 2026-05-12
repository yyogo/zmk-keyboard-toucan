#include <zephyr/kernel.h>
#include "battery.h"
#include "../assets/custom_fonts.h"

#define BATT_Y       12
#define BATT_BODY_W  42
#define BATT_BODY_H  12
#define BATT_NUB_W   2
#define BATT_NUB_H   4

#define BATT_LEFT_X         18
#define BATT_LEFT_LABEL_X    8

#define BATT_RIGHT_X        80
#define BATT_RIGHT_LABEL_X  (BATT_RIGHT_X + BATT_BODY_W + BATT_NUB_W + 3)

// ZMK's lithium_ion_mv_to_pct is a linear 3450..4200 mV mapping, so a
// fully-charged Li-Po (terminates at ~4.1..4.18 V at rest) reports 90%.
// Treat anything >= 90 as full so the display doesn't lie about it.
#define BATT_FULL_LEVEL 90

static void draw_label(lv_obj_t *canvas, int x, const char *label) {
    lv_draw_label_dsc_t dsc;
    init_label_dsc(&dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
    canvas_draw_text(canvas, x, BATT_Y + 1, 12, &dsc, label);
}

static void draw_outline(lv_obj_t *canvas, int x) {
    lv_draw_rect_dsc_t outline;
    lv_draw_rect_dsc_init(&outline);
    outline.bg_color = LVGL_BACKGROUND;
    outline.border_color = LVGL_FOREGROUND;
    outline.border_width = 1;
    canvas_draw_rect(canvas, x, BATT_Y, BATT_BODY_W, BATT_BODY_H, &outline);

    lv_draw_rect_dsc_t fill;
    init_rect_dsc(&fill, LVGL_FOREGROUND);
    canvas_draw_rect(canvas, x + BATT_BODY_W,
                        BATT_Y + (BATT_BODY_H - BATT_NUB_H) / 2,
                        BATT_NUB_W, BATT_NUB_H, &fill);
}

static void draw_fill(lv_obj_t *canvas, int x, uint8_t level) {
    if (level <= 1) return;
    if (level >= BATT_FULL_LEVEL) level = BATT_FULL_LEVEL;
    lv_draw_rect_dsc_t fill;
    init_rect_dsc(&fill, LVGL_FOREGROUND);
    int inner_w = BATT_BODY_W - 4;
    int inner_h = BATT_BODY_H - 4;
    int fill_w = (inner_w * level + BATT_FULL_LEVEL / 2) / BATT_FULL_LEVEL;
    if (fill_w > 0) {
        canvas_draw_rect(canvas, x + 2, BATT_Y + 2, fill_w, inner_h, &fill);
    }
}

// Single centered horizontal dash inside the outlined battery — reads as
// "no data" without the visual noise of an X or slash mark.
static void draw_offline(lv_obj_t *canvas, int x) {
    lv_draw_rect_dsc_t dash;
    init_rect_dsc(&dash, LVGL_FOREGROUND);
    int dash_w = 8;
    int dash_h = 2;
    int dash_gap = 2;
    int x0 = x + (BATT_BODY_W - dash_w * 3 - dash_gap * 2) / 2;
    int y0 = BATT_Y + (BATT_BODY_H - dash_h) / 2;
    for (int i = 0; i < 3; i++) {
        canvas_draw_rect(canvas, x0 + i * (dash_w + dash_gap), y0, dash_w, dash_h, &dash);
    }
}

static void draw_one(lv_obj_t *canvas, int x, int label_x, const char *label,
                     uint8_t level, bool offline) {
    draw_label(canvas, label_x, label);
    draw_outline(canvas, x);
    if (offline) {
        draw_offline(canvas, x);
    } else {
        draw_fill(canvas, x, level);
    }
}

void draw_battery_left(lv_obj_t *canvas, const struct status_state *state) {
    draw_one(canvas, BATT_LEFT_X, BATT_LEFT_LABEL_X, "L", state->battery, false);
}

void draw_battery_right(lv_obj_t *canvas, const struct status_state *state) {
    // ZMK's BLE central fires a peripheral_battery event with level=0 on
    // disconnect, so battery_p==0 doubles as the offline signal — there's
    // no separate central-side connection event we could subscribe to.
    bool offline = (state->battery_p == 0);
    draw_one(canvas, BATT_RIGHT_X, BATT_RIGHT_LABEL_X, "R",
             state->battery_p, offline);
}
