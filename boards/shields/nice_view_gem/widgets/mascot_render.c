// Pure drawing of the mascot sprite. No state, no ZMK deps, no fonts.
// Also compiled by the host-side preview at `preview/` against stub LVGL.
//
// Each shape is drawn as a stack of horizontal rects (one per row). The
// row-by-row spans were derived from the previous polygon-based version
// using the host preview's DUMP_SPANS=1 mode, then frozen here so the
// firmware and the host preview render byte-identically.
//
// Every rect goes through mr_rect(), which optionally mirrors its X
// coordinate around the body's vertical centerline so the entire sprite
// flips horizontally when flip_x is set.

#include <lvgl.h>

#include "mascot_render.h"

#define MASCOT_BG lv_color_black()
#define MASCOT_FG lv_color_white()

struct row_span {
    int8_t x;
    uint8_t w;
};

struct mr_ctx {
    lv_obj_t *canvas;
    int pivot;     // X column to mirror around (when flip is true)
    bool flip;
};

static inline void rect_dsc_color(lv_draw_rect_dsc_t *r, lv_color_t bg) {
    lv_draw_rect_dsc_init(r);
    r->bg_color = bg;
}

// Mirror a rect's left X around pivot (treating pivot as the integer column
// at the centerline -- so a rect from cols [x..x+w-1] maps to
// [2p - x - w + 1 .. 2p - x]).
static inline int mirror_x(int x, int w, int pivot) {
    return 2 * pivot - x - w + 1;
}

static void mr_rect(const struct mr_ctx *c, int x, int y, int w, int h,
                    const lv_draw_rect_dsc_t *dsc) {
    int x_out = c->flip ? mirror_x(x, w, c->pivot) : x;
    canvas_draw_rect(c->canvas, x_out, y, w, h, dsc);
}

static void draw_spans(const struct mr_ctx *c, int origin_x, int origin_y,
                       const struct row_span *rows, int n,
                       const lv_draw_rect_dsc_t *dsc) {
    for (int i = 0; i < n; i++) {
        if (rows[i].w == 0) {
            continue;
        }
        mr_rect(c, origin_x + rows[i].x, origin_y + i, rows[i].w, 1, dsc);
    }
}

// Punch a "dark" eye onto the (already white) head.
static void draw_eye(const struct mr_ctx *c, int x, int y,
                     bool closed, bool droopy, bool caret) {
    lv_draw_rect_dsc_t dot;
    rect_dsc_color(&dot, MASCOT_BG);
    if (closed) {
        mr_rect(c, x - 1, y + 1, 4, 1, &dot);
    } else if (droopy) {
        mr_rect(c, x, y + 2, 2, 2, &dot);
    } else if (caret) {
        mr_rect(c, x - 1, y + 1, 1, 1, &dot);
        mr_rect(c, x + 2, y + 1, 1, 1, &dot);
        mr_rect(c, x, y + 0, 2, 1, &dot);
    } else {
        mr_rect(c, x, y, 2, 2, &dot);
    }
}

static void draw_z(const struct mr_ctx *c, int x, int y) {
    lv_draw_rect_dsc_t fill;
    rect_dsc_color(&fill, MASCOT_FG);
    mr_rect(c, x + 0, y + 0, 4, 1, &fill);
    mr_rect(c, x + 2, y + 1, 1, 1, &fill);
    mr_rect(c, x + 1, y + 2, 1, 1, &fill);
    mr_rect(c, x + 0, y + 3, 4, 1, &fill);
}

// ---- Frozen shape spans ------------------------------------------------

// Head: chamfered octagon, 7 rows tall.
static const struct row_span head_rows[] = {
    { 2, 5 },
    { 1, 8 },
    { 0, 9 },
    { 0, 9 },
    { 0, 9 },
    { 0, 9 },
    { 0, 6 },
};

// Beak (4 rows) starting at hy+2.
static const struct row_span beak_rows[] = {
    { 8,  5 },
    { 8,  7 },
    { 8,  9 },
    { 8, 10 },
};

// Body, 9 rows for unsquashed (bh=10). The squash variant drops index 6
// for an 8-row body.
static const struct row_span body_rows[] = {
    { 2,  7 },
    { 2,  8 },
    { 1, 10 },
    { 1, 10 },
    { 0, 11 },
    { 0, 11 },
    { 0, 10 },  // dropped when squash
    { 0,  9 },
    { 1,  7 },
};

// Tail (4 rows) anchored at by+bh-4.
static const struct row_span tail_rows[] = {
    {  1, 1 },
    { -1, 4 },
    { -3, 6 },
    { -6, 5 },
};

// Happy frame B left flap (5 rows) starting at by+1.
static const struct row_span left_flap_rows[] = {
    { -7, 3 },
    { -6, 5 },
    { -6, 8 },
    { -5, 7 },
    { -4, 6 },
    { -2, 4 },
};

// Happy frame B right flap (5 rows) starting at by+0.
static const struct row_span right_flap_rows[] = {
    {  8, 3 },
    {  7, 3 },
    {  6, 3 },
    // {  , 6 },
    // {  6, 4 },
};

// ---- Render -----------------------------------------------------------

void mascot_render(lv_obj_t *canvas, int x, int y,
                   enum mascot_state state, int frame,
                   bool sleeping, bool reacting, bool flip_x, bool moving) {
    int body_y_offset = (state == MASCOT_SAD) ? 1 : 0;
    int feet_extra = (state == MASCOT_HAPPY) ? 1 : 0;
    int squash = (frame == 1 && !sleeping) ? 1 : 0;

    int bx = x + 6;
    struct mr_ctx ctx = {
        .canvas = canvas,
        // Body's widest rows are 11 wide starting at bx, so the centerline
        // sits at column bx + 5. Mirroring around that leaves the body
        // bounding box in place; the head + beak + tail swap sides.
        .pivot = bx + 5,
        .flip  = flip_x,
    };

    lv_draw_rect_dsc_t fill;
    rect_dsc_color(&fill, MASCOT_FG);

    // ----- Head -----
    int hx = x + 8;
    int hy = y + 5 + body_y_offset;
    draw_spans(&ctx, hx, hy, head_rows,
               (int)(sizeof(head_rows) / sizeof(head_rows[0])), &fill);

    // ----- Body (squash drops the second-widest middle row) -----
    int by = y + 12 + body_y_offset;
    int bh = 10 - squash;
    if (squash) {
        for (int i = 0; i <= 4; i++) {
            mr_rect(&ctx, bx + body_rows[i].x, by + i,
                    body_rows[i].w, 1, &fill);
        }
        for (int i = 5; i < 8; i++) {
            mr_rect(&ctx, bx + body_rows[i + 1].x, by + i,
                    body_rows[i + 1].w, 1, &fill);
        }
    } else {
        draw_spans(&ctx, bx, by, body_rows,
                   (int)(sizeof(body_rows) / sizeof(body_rows[0])), &fill);
    }

    // ----- Tail (anchored at by+bh-4) -----
    draw_spans(&ctx, bx, by + bh - 4, tail_rows,
               (int)(sizeof(tail_rows) / sizeof(tail_rows[0])), &fill);

    // ----- Beak -----
    draw_spans(&ctx, hx, hy + 2, beak_rows,
               (int)(sizeof(beak_rows) / sizeof(beak_rows[0])), &fill);

    // ----- Wing(s) -----
    if (!sleeping) {
        if (state == MASCOT_HAPPY && frame == 1) {
            draw_spans(&ctx, bx + 1, by - 2, left_flap_rows,
                       (int)(sizeof(left_flap_rows) / sizeof(left_flap_rows[0])), &fill);
            draw_spans(&ctx, bx + 3, by - 1, right_flap_rows,
                       (int)(sizeof(right_flap_rows) / sizeof(right_flap_rows[0])), &fill);
        }
    }

    // ----- Eye -----
    bool eye_closed = reacting || state == MASCOT_SLEEP;
    bool eye_droopy = (state == MASCOT_SAD) && !eye_closed;
    bool eye_caret = state == MASCOT_HAPPY && frame == 1 && !eye_closed;
    draw_eye(&ctx, hx + 3, hy + 2, eye_closed, eye_droopy, eye_caret);

    // ----- Sleep marks -----
    if (sleeping || state == MASCOT_SLEEP) {
        int z_shift_x = frame ? 2 : 0;
        int z_shift_y = frame ? -1 : 0;
        draw_z(&ctx, hx + 15 + z_shift_x, hy - 3 + z_shift_y);
    }

    // ----- Feet -----
    if (state != MASCOT_SLEEP) {
        int feet_y = y + 19;
        int feet_h = 3 + feet_extra;
        int left_x = bx + 2;
        int right_x = bx + 6;
        // Walking shuffle: when the bird is moving, alternate which foot is
        // "lifted" (1 px shorter) between breathing frames. Each foot's toe
        // pad rides at the bottom of its leg, so the lifted foot's pad
        // rises 1 px and the bird visibly steps. When stationary, both feet
        // stay grounded.
        int left_lift  = (moving && frame == 1) ? 1 : 0;
        int right_lift = (moving && frame == 0) ? 1 : 0;
        int left_h  = feet_h - left_lift;
        int right_h = feet_h - right_lift;
        mr_rect(&ctx, left_x,  feet_y, 2, left_h,  &fill);
        mr_rect(&ctx, right_x, feet_y, 2, right_h, &fill);
        mr_rect(&ctx, left_x - 1,  feet_y + left_h,  4, 1, &fill);
        mr_rect(&ctx, right_x - 1, feet_y + right_h, 6, 1, &fill);
    }
}
