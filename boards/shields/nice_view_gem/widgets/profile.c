#include <zephyr/kernel.h>
#include <zmk/ble.h>
#include "profile.h"

// 5 slots, 8x8 each, 2px gap (matches the pre-existing layout so other
// widgets stay aligned).
#define SLOT_X0     85
#define SLOT_Y0     143
#define SLOT_W      8
#define SLOT_H      8
#define SLOT_STRIDE 10

static inline void set_px(lv_obj_t *canvas, int x, int y) {
    lv_canvas_set_px_color(canvas, x, y, LVGL_FOREGROUND);
}

// Active + connected: solid fill.
static void draw_filled(lv_obj_t *canvas, int x) {
    lv_draw_rect_dsc_t dsc;
    init_rect_dsc(&dsc, LVGL_FOREGROUND);
    lv_canvas_draw_rect(canvas, x, SLOT_Y0, SLOT_W, SLOT_H, &dsc);
}

// Paired, not active: solid 1px outline.
static void draw_outline(lv_obj_t *canvas, int x) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = LVGL_BACKGROUND;
    dsc.border_color = LVGL_FOREGROUND;
    dsc.border_width = 1;
    lv_canvas_draw_rect(canvas, x, SLOT_Y0, SLOT_W, SLOT_H, &dsc);
}

// Unpaired slot: dashed outline (every other pixel of the perimeter).
static void draw_dashed(lv_obj_t *canvas, int x) {
    for (int i = 0; i < SLOT_W; i += 2) {
        set_px(canvas, x + i, SLOT_Y0);
        set_px(canvas, x + i + 1 - (SLOT_H % 2), SLOT_Y0 + SLOT_H - 1);
    }
    for (int i = 0; i < SLOT_H; i += 2) {
        set_px(canvas, x,                SLOT_Y0 + i);
        set_px(canvas, x + SLOT_W - 1,   SLOT_Y0 + i + 1 - (SLOT_W % 2));
    }
}

// Active but disconnected: checkerboard fill -- clearly distinct from
// both the solid-fill ("connected") and the empty outline ("inactive")
// states on a 1-bit display.
static void draw_dithered(lv_obj_t *canvas, int x) {
    for (int dy = 0; dy < SLOT_H; dy++) {
        for (int dx = 0; dx < SLOT_W; dx++) {
            if (((dx + dy) & 1) == 0) {
                set_px(canvas, x + dx, SLOT_Y0 + dy);
            }
        }
    }
}

void draw_profile_status(lv_obj_t *canvas, const struct status_state *state) {
    for (int i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        int x = SLOT_X0 + i * SLOT_STRIDE;
        bool is_active = (i == state->active_profile_index);
        bool is_open   = state->profile_open[i];

        if (is_open) {
            draw_dashed(canvas, x);
        } else {
            draw_outline(canvas, x);
        }
        if (is_active) {
            if (state->active_profile_connected) {
                draw_filled(canvas, x);
            } else {
                draw_dithered(canvas, x);
            }
        }
    }
}
