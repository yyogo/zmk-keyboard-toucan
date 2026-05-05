#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <stdio.h>
#include <string.h>

#include "layer.h"
#include "../assets/custom_fonts.h"
#include <zmk/physical_layouts.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>



static void draw_layer_indicator(lv_obj_t *canvas, const struct status_state *state) {
    const int n_layers = ZMK_KEYMAP_LAYERS_LEN;
    if (n_layers <= 1) {
        return;
    }

    const int rect_w = 16;
    const int rect_h = 5;
    const int gap = 2;
    const int total_w = n_layers * rect_w + (n_layers - 1) * gap;
    const int x_start = (SCREEN_WIDTH - total_w) / 2;
    const int y = 100;

    lv_draw_rect_dsc_t filled;
    init_rect_dsc(&filled, LVGL_FOREGROUND);

    lv_draw_rect_dsc_t outline;
    lv_draw_rect_dsc_init(&outline);
    outline.bg_color = LVGL_BACKGROUND;
    outline.border_color = LVGL_FOREGROUND;
    outline.border_width = 1;

    for (int i = 0; i < n_layers; i++) {
        int x = x_start + i * (rect_w + gap);
        if (i == state->layer_index) {
            lv_canvas_draw_rect(canvas, x, y, rect_w, rect_h, &filled);
        } else {
            lv_canvas_draw_rect(canvas, x, y, rect_w, rect_h, &outline);
        }
    }
}

void draw_layer_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_24, LV_TEXT_ALIGN_CENTER);

    char fallback_layer_name[16];

    const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state->layer_index));

    if (layer_name == NULL || layer_name[0] == '\0') {
        sprintf(fallback_layer_name, "L#%" PRIu8, state->layer_index);

        layer_name = fallback_layer_name;
    }

    lv_canvas_draw_text(canvas, 0, 70, SCREEN_WIDTH, &label_dsc, layer_name);
    draw_layer_indicator(canvas, state);
}
