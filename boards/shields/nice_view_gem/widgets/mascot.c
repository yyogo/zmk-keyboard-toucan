#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/display.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/activity_state_changed.h>

#include "mascot.h"
#include "mascot_render.h"
#include "../assets/custom_fonts.h"

extern void zmk_widget_screen_force_redraw(void);

// ----- Layout (origin at MASCOT_X, MASCOT_Y; sprite ~32x24) ------------

#define MASCOT_X 56
#define MASCOT_Y 110

// ----- Mood model ------------------------------------------------------

#define MOOD_HAPPY  30
#define MOOD_SAD   -30
#define MOOD_MAX   100
#define MOOD_MIN  -100

#define ANIM_PERIOD_MS    250
#define DECAY_PERIOD_MS   60000
#define REACTION_HOLD_MS  200

#define HIGH_WPM_THRESHOLD       80
#define HIGH_WPM_HOLD_MS         5000

#define WALK_STEP_MS  1000
#define WALK_RANGE    2

static int8_t s_mood = 0;
static bool s_sleeping = false;
static uint8_t s_frame_idx = 0;
static int64_t s_last_keypress_ms = 0;
static int64_t s_high_wpm_started_ms = 0;
static int8_t s_walk_offset = 0;
static int8_t s_walk_dir = 1;
static int64_t s_last_step_ms = 0;

static lv_timer_t *s_anim_timer = NULL;
static lv_timer_t *s_decay_timer = NULL;

static int clamp_mood(int v) {
    if (v > MOOD_MAX) return MOOD_MAX;
    if (v < MOOD_MIN) return MOOD_MIN;
    return v;
}

static enum mascot_state derive_state(void) {
    if (s_sleeping) return MASCOT_SLEEP;
    if (s_mood >= MOOD_HAPPY) return MASCOT_HAPPY;
    if (s_mood <= MOOD_SAD) return MASCOT_SAD;
    return MASCOT_NEUTRAL;
}

static bool is_reacting(void) {
    return s_last_keypress_ms != 0 &&
           (k_uptime_get() - s_last_keypress_ms) < REACTION_HOLD_MS;
}

// ----- LVGL timers (run in display thread) -----------------------------

static void anim_timer_cb(lv_timer_t *t) {
    if (s_sleeping) return;
    s_frame_idx ^= 1;
    zmk_widget_screen_force_redraw();
}

static void decay_timer_cb(lv_timer_t *t) {
    if (s_sleeping) return;
    int new_mood = clamp_mood(s_mood - 1);
    if (new_mood != s_mood) {
        s_mood = new_mood;
        zmk_widget_screen_force_redraw();
    }
}

// ----- Event listeners (callbacks run in display thread) ---------------

struct mascot_position_event {
    bool pressed;
};

static void mascot_position_cb(struct mascot_position_event ev) {
    if (!ev.pressed) {
        return;
    }
    int64_t now = k_uptime_get();
    s_mood = clamp_mood(s_mood + 1);
    s_last_keypress_ms = now;

    // Take a 1 px walking step at most once per WALK_STEP_MS while typing,
    // pacing back-and-forth within +/- WALK_RANGE.
    if (now - s_last_step_ms >= WALK_STEP_MS) {
        s_walk_offset += s_walk_dir;
        if (s_walk_offset >= WALK_RANGE) {
            s_walk_offset = WALK_RANGE;
            s_walk_dir = -1;
        } else if (s_walk_offset <= -WALK_RANGE) {
            s_walk_offset = -WALK_RANGE;
            s_walk_dir = 1;
        }
        s_last_step_ms = now;
    }

    zmk_widget_screen_force_redraw();
}

static struct mascot_position_event mascot_position_get(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *p = as_zmk_position_state_changed(eh);
    return (struct mascot_position_event){
        .pressed = (p != NULL && p->state),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(mascot_position_listener, struct mascot_position_event,
                            mascot_position_cb, mascot_position_get)
ZMK_SUBSCRIPTION(mascot_position_listener, zmk_position_state_changed);

struct mascot_wpm_event {
    int wpm;
};

static void mascot_wpm_cb(struct mascot_wpm_event ev) {
    int64_t now = k_uptime_get();
    if (ev.wpm >= HIGH_WPM_THRESHOLD) {
        if (s_high_wpm_started_ms == 0) {
            s_high_wpm_started_ms = now;
        } else if (now - s_high_wpm_started_ms >= HIGH_WPM_HOLD_MS) {
            s_mood = clamp_mood(s_mood - 5);
            s_high_wpm_started_ms = now;
            zmk_widget_screen_force_redraw();
        }
    } else {
        s_high_wpm_started_ms = 0;
    }
}

static struct mascot_wpm_event mascot_wpm_get(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *p = as_zmk_wpm_state_changed(eh);
    return (struct mascot_wpm_event){
        .wpm = (p != NULL) ? p->state : 0,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(mascot_wpm_listener, struct mascot_wpm_event,
                            mascot_wpm_cb, mascot_wpm_get)
ZMK_SUBSCRIPTION(mascot_wpm_listener, zmk_wpm_state_changed);

struct mascot_activity_event {
    bool sleeping;
};

static void mascot_activity_cb(struct mascot_activity_event ev) {
    if (ev.sleeping == s_sleeping) {
        return;
    }
    s_sleeping = ev.sleeping;
    if (s_sleeping) {
        // Snap back to center so wake-up is consistent.
        s_walk_offset = 0;
    }
    zmk_widget_screen_force_redraw();
}

static struct mascot_activity_event mascot_activity_get(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *p = as_zmk_activity_state_changed(eh);
    return (struct mascot_activity_event){
        .sleeping = (p != NULL && p->state == ZMK_ACTIVITY_SLEEP),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(mascot_activity_listener, struct mascot_activity_event,
                            mascot_activity_cb, mascot_activity_get)
ZMK_SUBSCRIPTION(mascot_activity_listener, zmk_activity_state_changed);

// ----- Init -----------------------------------------------------------

void mascot_widget_init(void) {
    mascot_position_listener_init();
    mascot_wpm_listener_init();
    mascot_activity_listener_init();
    s_anim_timer = lv_timer_create(anim_timer_cb, ANIM_PERIOD_MS, NULL);
    s_decay_timer = lv_timer_create(decay_timer_cb, DECAY_PERIOD_MS, NULL);
}

// ----- Drawing --------------------------------------------------------

void draw_mascot(lv_obj_t *canvas, const struct status_state *state) {
    enum mascot_state ms = derive_state();
    bool reacting = !s_sleeping && is_reacting();

    int draw_x = MASCOT_X + s_walk_offset;
    mascot_render(canvas, draw_x, MASCOT_Y, ms, s_frame_idx, s_sleeping, reacting);

    // Sleep "z" overlay (kept here because it needs the font asset, which
    // mascot_render.c intentionally avoids depending on).
    if (ms == MASCOT_SLEEP) {
        lv_draw_label_dsc_t label_dsc;
        init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
        lv_canvas_draw_text(canvas, draw_x + 5 + 9 + 4, MASCOT_Y + 1, 8, &label_dsc, "z");
    }
}
