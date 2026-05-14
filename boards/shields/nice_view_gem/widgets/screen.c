#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/endpoints.h>
#include <zmk/hid_indicators.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/split/central.h>

#include <toucan/events/caps_word_state_changed.h>

#include "battery.h"
#include "boot_logo.h"
#include "caps.h"
#include "layer.h"
#include "mascot.h"
#include "output.h"
#include "profile.h"
#include "screen.h"
#include "sleep.h"

struct connection_status_state {
    bool connected;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/**
 * Draw buffers
 **/

static void draw_top(lv_obj_t *widget, uint8_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    fill_background(canvas);

    if (is_sleep_screen_active()) {
        draw_sleep_screen(canvas);
        return;
    }

    if (is_boot_logo_active()) {
        draw_boot_logo(canvas);
        return;
    }

    // Draw widgets
    draw_output_status(canvas, state);
    draw_layer_status(canvas, state);
    draw_profile_status(canvas, state);
    draw_battery_left(canvas, state);
    draw_battery_right(canvas, state);
    draw_caps_status(canvas, state);
    draw_mascot(canvas, state);
}

/**
 * Battery status
 **/
// L
static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    widget->state.battery = state.level;

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state);

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

// R
static void set_battery_peripheral_status(struct zmk_widget_screen *widget,
                               struct battery_peripheral_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging_p = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    uint8_t level;
    zmk_split_central_get_peripheral_battery_level(0, &level);

    widget->state.battery_p = level;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_peripheral_status_update_cb(struct battery_peripheral_status_state state) {
    struct zmk_widget_screen *widget;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_peripheral_status(widget, state); }
}

static struct battery_peripheral_status_state battery_peripheral_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);


    return (struct battery_peripheral_status_state){
        .level = ev->state_of_charge,
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_peripheral_status, struct battery_peripheral_status_state,
                            battery_peripheral_status_update_cb, battery_peripheral_status_get_state);

ZMK_SUBSCRIPTION(widget_battery_peripheral_status, zmk_peripheral_battery_state_changed);

/**
 * Layer status
 **/

static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state) {
    widget->state.layer_index = zmk_keymap_highest_layer_active();
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state) {
        .index = index
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/**
 * Caps Lock indicator (host HID indicator bit 1)
 **/

struct caps_status_state {
    bool caps_lock;
};

static void set_caps_status(struct zmk_widget_screen *widget, struct caps_status_state state) {
    widget->state.caps_lock = state.caps_lock;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void caps_status_update_cb(struct caps_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_caps_status(widget, state); }
}

static struct caps_status_state caps_status_get_state(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    zmk_hid_indicators_t indicators =
        (ev != NULL) ? ev->indicators : zmk_hid_indicators_get_current_profile();
    return (struct caps_status_state){
        .caps_lock = (indicators & 0x02) != 0,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_caps_status, struct caps_status_state, caps_status_update_cb,
                            caps_status_get_state)

ZMK_SUBSCRIPTION(widget_caps_status, zmk_hid_indicators_changed);

/**
 * Caps Word indicator
 **/

struct caps_word_status_state {
    bool active;
};

static void set_caps_word_status(struct zmk_widget_screen *widget,
                                 struct caps_word_status_state state) {
    widget->state.caps_word = state.active;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void caps_word_status_update_cb(struct caps_word_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_caps_word_status(widget, state); }
}

static struct caps_word_status_state caps_word_status_get_state(const zmk_event_t *eh) {
    const struct toucan_caps_word_state_changed *ev = as_toucan_caps_word_state_changed(eh);
    return (struct caps_word_status_state){
        .active = (ev != NULL) ? ev->active : false,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_caps_word_status, struct caps_word_status_state,
                            caps_word_status_update_cb, caps_word_status_get_state)

ZMK_SUBSCRIPTION(widget_caps_word_status, toucan_caps_word_state_changed);

/**
 * Output status
 **/

static void set_output_status(struct zmk_widget_screen *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;
    memcpy(widget->state.profile_open, state->profile_open,
           sizeof(state->profile_open));

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    struct output_status_state s = {
        .selected_endpoint = zmk_endpoint_get_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
    for (int i = 0; i < ZMK_BLE_PROFILE_COUNT; i++) {
        s.profile_open[i] = zmk_ble_profile_is_open(i);
    }
    return s;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/**
 * Activity state handling for sleep screen
 **/

#define SLEEP_SCREEN_FLUSH_TIMEOUT_MS 250

static void force_redraw_all_widgets(void) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_top(widget->obj, widget->cbuf, &widget->state);
    }
}

void zmk_widget_screen_force_redraw(void) {
    force_redraw_all_widgets();
}

static void active_screen_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    bool was_sleeping = is_sleep_screen_active();
    set_sleep_screen_active(false);
    if (was_sleeping) {
        start_boot_logo();
        force_redraw_all_widgets();
    }
}

K_WORK_DEFINE(active_screen_work, active_screen_work_cb);

K_SEM_DEFINE(sleep_screen_flushed, 0, 1);

static void sleep_screen_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    set_sleep_screen_active(true);
    force_redraw_all_widgets();

    // The sleep transition immediately powers off after listeners return.
    // Flush from the display queue so the Sharp panel receives the sleep frame
    // before sys_poweroff().
    lv_task_handler();
    lv_refr_now(NULL);
    k_sem_give(&sleep_screen_flushed);
}

K_WORK_DEFINE(sleep_screen_work, sleep_screen_work_cb);

static void drain_sleep_screen_flush_sem(void) {
    while (k_sem_take(&sleep_screen_flushed, K_NO_WAIT) == 0) {
    }
}

static int display_activity_event_handler(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return -ENOTSUP;
    }

    if (!zmk_display_is_initialized()) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_ACTIVE:
#if IS_ENABLED(CONFIG_ZMK_DISPLAY_WORK_QUEUE_DEDICATED)
        k_work_submit_to_queue(zmk_display_work_q(), &active_screen_work);
#else
        active_screen_work_cb(NULL);
#endif
        break;
    case ZMK_ACTIVITY_SLEEP: {
        drain_sleep_screen_flush_sem();
#if IS_ENABLED(CONFIG_ZMK_DISPLAY_WORK_QUEUE_DEDICATED)
        int submitted = k_work_submit_to_queue(zmk_display_work_q(), &sleep_screen_work);
        if (submitted >= 0) {
            (void)k_sem_take(&sleep_screen_flushed, K_MSEC(SLEEP_SCREEN_FLUSH_TIMEOUT_MS));
        }
#else
        sleep_screen_work_cb(NULL);
#endif
        break;
    }
    default:
        break; // ignore other states (like IDLE)
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(nice_view_gem_display, display_activity_event_handler);
ZMK_SUBSCRIPTION(nice_view_gem_display, zmk_activity_state_changed);

/**
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, SCREEN_WIDTH, SCREEN_HEIGHT);

    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, SCREEN_WIDTH, SCREEN_HEIGHT, CANVAS_COLOR_FORMAT);

    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_battery_peripheral_status_init();
    widget_layer_status_init();
    widget_output_status_init();
    widget_caps_status_init();
    widget_caps_word_status_init();
    mascot_widget_init();
    start_boot_logo();

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
