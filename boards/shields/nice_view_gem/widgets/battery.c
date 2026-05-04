#include <zephyr/kernel.h>
#include "battery.h"
#include "../assets/custom_fonts.h"

// Left-side battery indicator drawn from primitives (mirrors the
// peripheral battery on the right).

#define BATT_X       18
#define BATT_Y       12
#define BATT_BODY_W  42
#define BATT_BODY_H  12
#define BATT_NUB_W   2
#define BATT_NUB_H   4
#define BATT_LABEL_X 8
#define BATT_LABEL_Y (BATT_Y + 1)

static void draw_level(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
    lv_canvas_draw_text(canvas, BATT_LABEL_X, BATT_LABEL_Y, 12, &label_dsc, "L");

    lv_draw_rect_dsc_t outline;
    lv_draw_rect_dsc_init(&outline);
    outline.bg_color = LVGL_BACKGROUND;
    outline.border_color = LVGL_FOREGROUND;
    outline.border_width = 1;
    lv_canvas_draw_rect(canvas, BATT_X, BATT_Y, BATT_BODY_W, BATT_BODY_H, &outline);

    lv_draw_rect_dsc_t fill;
    init_rect_dsc(&fill, LVGL_FOREGROUND);
    lv_canvas_draw_rect(canvas, BATT_X + BATT_BODY_W,
                        BATT_Y + (BATT_BODY_H - BATT_NUB_H) / 2,
                        BATT_NUB_W, BATT_NUB_H, &fill);

    uint8_t level = state->battery;
    if (level > 1) {
        int inner_w = BATT_BODY_W - 4;
        int inner_h = BATT_BODY_H - 4;
        int fill_w = (inner_w * level + 50) / 100;
        if (fill_w > 0) {
            lv_canvas_draw_rect(canvas, BATT_X + 2, BATT_Y + 2, fill_w, inner_h, &fill);
        }
    }
}

void draw_battery_status(lv_obj_t *canvas, const struct status_state *state) {
    draw_level(canvas, state);
}
