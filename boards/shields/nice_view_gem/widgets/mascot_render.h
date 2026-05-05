#pragma once

#include <lvgl.h>

enum mascot_state {
    MASCOT_HAPPY,
    MASCOT_NEUTRAL,
    MASCOT_SAD,
    MASCOT_SLEEP,
};

// Paints the mascot into `canvas`, top-left of the sprite at (x, y).
// Pure drawing — no globals, no ZMK deps. Suitable for a host preview.
//   state    - which expression to render
//   frame    - 0 or 1 (idle "breathing" frame variation)
//   sleeping - if true, body+animation are at rest (no feet, no breathing)
//   reacting - if true, eye is closed (wink reaction)
void mascot_render(lv_obj_t *canvas, int x, int y,
                   enum mascot_state state, int frame,
                   bool sleeping, bool reacting);
