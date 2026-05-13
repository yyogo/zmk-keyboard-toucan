#include <zephyr/kernel.h>

#include "caps.h"
#include "../assets/custom_fonts.h"

#define CAPS_STATUS_X 8
#define CAPS_STATUS_Y 45

void draw_caps_status(lv_obj_t *canvas, const struct status_state *state) {
    if (!state->caps_lock) {
        return;
    }
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
    canvas_draw_text(canvas, CAPS_STATUS_X, CAPS_STATUS_Y, SCREEN_WIDTH, &label_dsc, "CAPS");
}
