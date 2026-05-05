// Pure drawing of the mascot sprite. No state, no ZMK deps, no fonts.
// This file is also compiled by the host-side preview tool at
// `preview/` in the umbrella repo, against stub LVGL types.

#include <lvgl.h>

#include "mascot_render.h"

#define MASCOT_BG lv_color_black()
#define MASCOT_FG lv_color_white()

static inline void rect_dsc_color(lv_draw_rect_dsc_t *r, lv_color_t bg) {
    lv_draw_rect_dsc_init(r);
    r->bg_color = bg;
}

// Punch a "dark" eye onto the (already white) head.
static void draw_eye(lv_obj_t *canvas, int x, int y, bool closed, bool droopy) {
    lv_draw_rect_dsc_t dot;
    rect_dsc_color(&dot, MASCOT_BG);
    if (closed) {
        lv_canvas_draw_rect(canvas, x - 1, y + 1, 4, 1, &dot);
    } else if (droopy) {
        lv_canvas_draw_rect(canvas, x, y + 2, 2, 2, &dot);
    } else {
        lv_canvas_draw_rect(canvas, x, y, 2, 2, &dot);
    }
}

// Small 3x2 caret eye:
// # #
//  #
static void draw_eye_caret(lv_obj_t *canvas, int x, int y) {
    lv_draw_rect_dsc_t dot;
    rect_dsc_color(&dot, MASCOT_BG);
    lv_canvas_draw_rect(canvas, x + 0, y + 1, 1, 1, &dot);
    lv_canvas_draw_rect(canvas, x + 2, y + 1, 1, 1, &dot);
    lv_canvas_draw_rect(canvas, x + 1, y + 0, 1, 1, &dot);
}

static void draw_z(lv_obj_t *canvas, int x, int y) {
    lv_draw_rect_dsc_t fill;
    rect_dsc_color(&fill, MASCOT_FG);
    lv_canvas_draw_rect(canvas, x + 0, y + 0, 4, 1, &fill);
    lv_canvas_draw_rect(canvas, x + 2, y + 1, 1, 1, &fill);
    lv_canvas_draw_rect(canvas, x + 1, y + 2, 1, 1, &fill);
    lv_canvas_draw_rect(canvas, x + 0, y + 3, 4, 1, &fill);
}

void mascot_render(lv_obj_t *canvas, int x, int y,
                   enum mascot_state state, int frame,
                   bool sleeping, bool reacting) {
    int body_y_offset = 0;
    int feet_extra = 0;
    if (state == MASCOT_SAD) {
        body_y_offset = 1;
    } else if (state == MASCOT_HAPPY) {
        body_y_offset = -1;
        feet_extra = 1;       // perky tiptoes
    }
    int squash = (frame == 1 && !sleeping) ? 1 : 0;

    lv_draw_rect_dsc_t fill;
    rect_dsc_color(&fill, MASCOT_FG);
    lv_draw_rect_dsc_t bg;
    rect_dsc_color(&bg, MASCOT_BG);

    // ----- Head: chamfered octagon, 9x9 -----
    int hx = x + 5;
    int hy = y + 4 + body_y_offset;
    lv_point_t head_pts[] = {
        { hx + 1, hy + 0 },
        { hx + 7, hy + 0 },
        { hx + 8, hy + 1 },
        { hx + 8, hy + 7 },
        { hx + 7, hy + 8 },
        { hx + 1, hy + 8 },
        { hx + 0, hy + 7 },
        { hx + 0, hy + 1 },
    };
    lv_canvas_draw_polygon(canvas, head_pts, 8, &fill);

    // ----- Body: compact bird torso, 12 wide x 10 tall (B frame squashes
    //       1 px from the bottom for a subtle breathing motion) -----
    int bx = x + 6;
    int by = y + 12 + body_y_offset;
    int bw = 12;
    int bh = 10 - squash;
    lv_point_t body_pts[] = {
        { bx + 4,      by + 0       },
        { bx + bw - 5, by + 0       },
        { bx + bw - 2, by + 2       },
        { bx + bw - 1, by + 5       },
        { bx + bw - 2, by + bh - 3  },
        { bx + bw - 4, by + bh - 1  },
        { bx + 2,      by + bh - 1  },
        { bx + 0,      by + bh - 3  },
        { bx + 0,      by + 2       },
    };
    lv_canvas_draw_polygon(canvas, body_pts, 9, &fill);

    // ----- Tail: slightly more pronounced rear wedge -----
    lv_point_t tail_pts[] = {
        { bx + 1, by + bh - 4 },
        { bx - 3, by + bh - 2 },
        { bx + 1, by + bh - 0 },
        { bx + 2, by + bh - 2 },
    };
    lv_canvas_draw_polygon(canvas, tail_pts, 4, &fill);

    // ----- Beak: shorter, curved profile from head's right side -----
    lv_point_t beak_pts[] = {
        { hx + 8,  hy + 2 },
        { hx + 13, hy + 1 },
        { hx + 18, hy + 3 },
        { hx + 17, hy + 4 },
        { hx + 13, hy + 5 },
        { hx + 8,  hy + 6 },
    };
    lv_canvas_draw_polygon(canvas, beak_pts, 6, &fill);

    // ----- Wing detail: filled wing bump (kept solid to avoid body "hole").
    //       Position alternates per frame so it still reads as a tiny flap. -----
    if (!sleeping) {
        if (state == MASCOT_HAPPY && frame == 1) {
            // Extra up-flap on happy frame B (both sides).
            // Screen-left bump makes the flap read more pronounced.
            lv_point_t left_flap_pts[] = {
                { bx + 1, by + 3 },
                { bx - 3, by + 1 },
                { bx - 5, by + 3 },
                { bx - 4, by + 5 },
                { bx - 1, by + 6 },
                { bx + 1, by + 5 },
            };
            lv_canvas_draw_polygon(canvas, left_flap_pts, 6, &fill);

            // Screen-right flap.
            lv_point_t flap_pts[] = {
                { bx + 5,      by + 2 },
                { bx + bw - 3, by + 0 },
                { bx + bw - 1, by + 2 },
                { bx + bw - 2, by + 4 },
                { bx + 7,      by + 5 },
                { bx + 4,      by + 3 },
            };
            lv_canvas_draw_polygon(canvas, flap_pts, 6, &fill);
        } else {
            int wing_y = by + 3 + frame;
            lv_point_t wing_pts[] = {
                { bx + 4,      wing_y + 0 },
                { bx + bw - 3, wing_y + 1 },
                { bx + bw - 1, wing_y + 3 },
                { bx + bw - 2, wing_y + 5 },
                { bx + 6,      wing_y + 5 },
                { bx + 3,      wing_y + 2 },
            };
            lv_canvas_draw_polygon(canvas, wing_pts, 6, &fill);
        }
    }

    // ----- Eye -----
    bool eye_closed = reacting || state == MASCOT_SLEEP;
    bool eye_droopy = (state == MASCOT_SAD) && !eye_closed;
    if (state == MASCOT_HAPPY && frame == 1 && !eye_closed) {
        draw_eye_caret(canvas, hx + 3, hy + 3);
    } else {
        draw_eye(canvas, hx + 3, hy + 3, eye_closed, eye_droopy);
    }

    // ----- Sleep marks: two tiny 3x3 Zs drifting between frames -----
    if (sleeping || state == MASCOT_SLEEP) {
        int z_shift_x = frame ? 2 : 0;
        int z_shift_y = frame ? -1 : 0;
        draw_z(canvas, hx + 15 + z_shift_x, hy - 3 + z_shift_y);
    }

    // ----- Feet (tucked under in sleep) -----
    if (state != MASCOT_SLEEP) {
        int feet_y = by + bh;
        int feet_h = 2 + feet_extra;
        int left_x = bx + 3;
        int right_x = bx + 7;
        lv_canvas_draw_rect(canvas, left_x, feet_y, 2, feet_h, &fill);
        lv_canvas_draw_rect(canvas, right_x, feet_y, 2, feet_h, &fill);
        // Tiny toe pads so legs read as feet.
        lv_canvas_draw_rect(canvas, left_x - 1, feet_y + feet_h - 1, 4, 1, &fill);
        lv_canvas_draw_rect(canvas, right_x - 1, feet_y + feet_h - 1, 4, 1, &fill);
    }
}
