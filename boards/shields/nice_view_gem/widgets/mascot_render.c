// Pure drawing of the mascot sprite. No state, no ZMK deps, no fonts.
// Also compiled by the host-side preview at `preview/` against stub LVGL.
//
// Each shape is drawn as a stack of horizontal rects (one per row). The
// row-by-row spans were derived from the previous polygon-based version
// using the host preview's DUMP_SPANS=1 mode, then frozen here so the
// firmware and the host preview render byte-identically.

#include <lvgl.h>

#include "mascot_render.h"

#define MASCOT_BG lv_color_black()
#define MASCOT_FG lv_color_white()

struct row_span {
    int8_t x;
    uint8_t w;
};

static inline void rect_dsc_color(lv_draw_rect_dsc_t *r, lv_color_t bg) {
    lv_draw_rect_dsc_init(r);
    r->bg_color = bg;
}

static void draw_spans(lv_obj_t *canvas, int origin_x, int origin_y,
                       const struct row_span *rows, int n,
                       const lv_draw_rect_dsc_t *dsc) {
    for (int i = 0; i < n; i++) {
        if (rows[i].w == 0) {
            continue;
        }
        lv_canvas_draw_rect(canvas, origin_x + rows[i].x, origin_y + i,
                            rows[i].w, 1, dsc);
    }
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

// Caret eye for happy frame B: " #" on top, "# #" below.
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

// ---- Frozen shape spans ------------------------------------------------

// Head: chamfered octagon, 8 rows tall (the polygon's max-y row was empty
// under LVGL's half-open edge rule, so it's 8 rows -- not 9).
static const struct row_span head_rows[] = {
    { 1, 7 },
    { 0, 9 },
    { 0, 9 },
    { 0, 9 },
    { 0, 9 },
    { 0, 9 },
    { 1, 8 },
    { 2, 7 },
};

// Body, 9 rows for unsquashed (bh=10). The squash variant drops index 6
// (second-widest middle row) for an 8-row body.
static const struct row_span body_rows[] = {
    { 4,  4 },
    { 2,  7 },
    { 0, 11 },
    { 0, 11 },
    { 0, 11 },
    { 0, 12 },
    { 0, 12 },  // dropped when squash
    { 0, 11 },
    { 1,  9 },
};

// Tail (4 rows) anchored at by+bh-4.
static const struct row_span tail_rows[] = {
    {  1, 1 },
    { -1, 4 },
    { -3, 6 },
    { -1, 3 },
};

// Beak (5 rows) starting at hy+2.
static const struct row_span beak_rows[] = {
    { 8,  5 },
    { 8,  7 },
    { 8,  9 },
    { 8, 11 },
    { 8, 10 },
};

// Idle wing (5 rows) starting at wing_y.
static const struct row_span wing_rows[] = {
    { 4, 1 },
    { 3, 7 },
    { 3, 8 },
    { 4, 8 },
    { 5, 7 },
};

// Happy frame B left flap (5 rows) starting at by+1.
static const struct row_span left_flap_rows[] = {
    { -3, 1 },
    { -4, 4 },
    { -5, 7 },
    { -5, 7 },
    { -4, 6 },
};

// Happy frame B right flap (5 rows) starting at by+0.
static const struct row_span right_flap_rows[] = {
    {  9, 1 },
    {  7, 4 },
    {  5, 7 },
    {  4, 8 },
    {  6, 5 },
};

// ---- Render -----------------------------------------------------------

void mascot_render(lv_obj_t *canvas, int x, int y,
                   enum mascot_state state, int frame,
                   bool sleeping, bool reacting) {
    int body_y_offset = (state == MASCOT_SAD) ? 1 : 0;
    int feet_extra = (state == MASCOT_HAPPY) ? 1 : 0;
    int squash = (frame == 1 && !sleeping) ? 1 : 0;

    lv_draw_rect_dsc_t fill;
    rect_dsc_color(&fill, MASCOT_FG);

    // ----- Head -----
    int hx = x + 7;
    int hy = y + 4 + body_y_offset;
    draw_spans(canvas, hx, hy, head_rows,
               (int)(sizeof(head_rows) / sizeof(head_rows[0])), &fill);

    // ----- Body (squash drops the second 12-wide middle row) -----
    int bx = x + 6;
    int by = y + 12 + body_y_offset;
    int bh = 10 - squash;
    if (squash) {
        // Rows 0..5 normally, then row 7..8 (skip the duplicate widest row 6).
        for (int i = 0; i <= 5; i++) {
            lv_canvas_draw_rect(canvas, bx + body_rows[i].x, by + i,
                                body_rows[i].w, 1, &fill);
        }
        for (int i = 6; i < 8; i++) {
            lv_canvas_draw_rect(canvas, bx + body_rows[i + 1].x, by + i,
                                body_rows[i + 1].w, 1, &fill);
        }
    } else {
        draw_spans(canvas, bx, by, body_rows,
                   (int)(sizeof(body_rows) / sizeof(body_rows[0])), &fill);
    }

    // ----- Tail (anchored at by+bh-4) -----
    draw_spans(canvas, bx, by + bh - 4, tail_rows,
               (int)(sizeof(tail_rows) / sizeof(tail_rows[0])), &fill);

    // ----- Beak -----
    draw_spans(canvas, hx, hy + 2, beak_rows,
               (int)(sizeof(beak_rows) / sizeof(beak_rows[0])), &fill);

    // ----- Wing(s) -----
    if (!sleeping) {
        if (state == MASCOT_HAPPY && frame == 1) {
            draw_spans(canvas, bx, by + 1, left_flap_rows,
                       (int)(sizeof(left_flap_rows) / sizeof(left_flap_rows[0])), &fill);
            draw_spans(canvas, bx, by + 0, right_flap_rows,
                       (int)(sizeof(right_flap_rows) / sizeof(right_flap_rows[0])), &fill);
        } else {
            int wing_y = by + 3 + frame;
            draw_spans(canvas, bx, wing_y, wing_rows,
                       (int)(sizeof(wing_rows) / sizeof(wing_rows[0])), &fill);
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

    // ----- Sleep marks -----
    if (sleeping || state == MASCOT_SLEEP) {
        int z_shift_x = frame ? 2 : 0;
        int z_shift_y = frame ? -1 : 0;
        draw_z(canvas, hx + 15 + z_shift_x, hy - 3 + z_shift_y);
    }

    // ----- Feet -----
    if (state != MASCOT_SLEEP) {
        int feet_y = y + 19;
        int feet_h = 4 + feet_extra;
        int left_x = bx + 2;
        int right_x = bx + 6;
        lv_canvas_draw_rect(canvas, left_x, feet_y, 2, feet_h, &fill);
        lv_canvas_draw_rect(canvas, right_x, feet_y, 2, feet_h, &fill);
        lv_canvas_draw_rect(canvas, left_x, feet_y + feet_h - 1, 4, 1, &fill);
        lv_canvas_draw_rect(canvas, right_x, feet_y + feet_h - 1, 4, 1, &fill);
    }
}
