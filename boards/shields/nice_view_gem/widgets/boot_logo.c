#include <zephyr/kernel.h>

#include "boot_logo.h"
#include "sleep.h"
#include "../assets/custom_fonts.h"

#define BOOT_LOGO_DURATION_MS 1000
#define BOOT_LOGO_SLIDE_MS    500
#define BOOT_LOGO_FRAME_MS    50

#define LOGO_W 96
#define LOGO_H 96
#define LOGO_FINAL_X ((SCREEN_WIDTH - LOGO_W) / 2)
#define LOGO_FINAL_Y 32

static int64_t boot_logo_start_ms = 0;

static void boot_logo_timer_cb(lv_timer_t *t);

bool is_boot_logo_active(void) {
    if (boot_logo_start_ms == 0) {
        return false;
    }
    return (k_uptime_get() - boot_logo_start_ms) < BOOT_LOGO_DURATION_MS;
}

void start_boot_logo(void) {
    boot_logo_start_ms = k_uptime_get();
    lv_timer_create(boot_logo_timer_cb, BOOT_LOGO_FRAME_MS, NULL);
}

void draw_boot_logo(lv_obj_t *canvas) {
    int64_t elapsed = k_uptime_get() - boot_logo_start_ms;

    int y;
    if (elapsed < BOOT_LOGO_SLIDE_MS) {
        // Slide up from bottom edge to final resting Y
        int32_t progress = (int32_t)((elapsed * 1000) / BOOT_LOGO_SLIDE_MS); // 0..1000
        y = SCREEN_HEIGHT - ((SCREEN_HEIGHT - LOGO_FINAL_Y) * progress) / 1000;
    } else {
        y = LOGO_FINAL_Y;
    }

    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    canvas_draw_img(canvas, LOGO_FINAL_X, y, &sleep_icon, &img_dsc);
}

// LVGL timer drives frame redraws while the animation is live; the timer
// deletes itself once the duration elapses, then triggers a final redraw so
// the regular status widgets take over.
extern void zmk_widget_screen_force_redraw(void);

static void boot_logo_timer_cb(lv_timer_t *t) {
    if (is_boot_logo_active()) {
        zmk_widget_screen_force_redraw();
        return;
    }
    boot_logo_start_ms = 0;
    zmk_widget_screen_force_redraw();
    lv_timer_del(t);
}
