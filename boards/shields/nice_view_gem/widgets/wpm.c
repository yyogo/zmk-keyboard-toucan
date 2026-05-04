#include <zephyr/kernel.h>
#include <stdio.h>

#include "wpm.h"
#include "../assets/custom_fonts.h"

void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
    if (state->wpm <= 0) {
        return;
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%d WPM", state->wpm);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);
    lv_canvas_draw_text(canvas, 0, 55, SCREEN_WIDTH, &label_dsc, buf);
}
