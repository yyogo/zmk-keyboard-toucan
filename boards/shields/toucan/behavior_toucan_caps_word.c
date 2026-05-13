#define DT_DRV_COMPAT toucan_behavior_caps_word

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>

#include <toucan/events/caps_word_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

ZMK_EVENT_IMPL(toucan_caps_word_state_changed);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct caps_word_continue_item {
    uint16_t page;
    uint32_t id;
    uint8_t implicit_modifiers;
};

struct behavior_toucan_caps_word_config {
    uint8_t continuations_count;
    struct caps_word_continue_item continuations[];
};

struct behavior_toucan_caps_word_data {
    bool active;
};

#define GET_DEV(inst) DEVICE_DT_INST_GET(inst),
static const struct device *devs[] = {DT_INST_FOREACH_STATUS_OKAY(GET_DEV)};

static bool any_caps_word_active(void) {
    for (int i = 0; i < ARRAY_SIZE(devs); i++) {
        const struct device *dev = devs[i];
        struct behavior_toucan_caps_word_data *data = dev->data;
        if (data->active) {
            return true;
        }
    }

    return false;
}

static void set_caps_word_active(const struct device *dev, bool active) {
    struct behavior_toucan_caps_word_data *data = dev->data;

    if (data->active == active) {
        return;
    }

    data->active = active;
    raise_toucan_caps_word_state_changed(
        (struct toucan_caps_word_state_changed){.active = any_caps_word_active()});
}

static void deactivate_caps_word(const struct device *dev) { set_caps_word_active(dev, false); }

// Delegate the actual key handling to ZMK's built-in Caps Word behavior.
// This wrapper only mirrors state for the display, so listener ordering
// cannot affect whether alpha keys get shifted.
static int invoke_zmk_caps_word(struct zmk_behavior_binding_event event, bool pressed) {
    static const struct zmk_behavior_binding binding = {
        .behavior_dev = "caps_word",
        .param1 = 0,
        .param2 = 0,
    };

    return zmk_behavior_invoke_binding(&binding, event, pressed);
}

static int on_caps_word_binding_pressed(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_toucan_caps_word_data *data = dev->data;
    bool next_active = !data->active;

    int ret = invoke_zmk_caps_word(event, true);
    if (ret != ZMK_BEHAVIOR_OPAQUE) {
        return ret;
    }

    set_caps_word_active(dev, next_active);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_caps_word_binding_released(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    int ret = invoke_zmk_caps_word(event, false);
    if (ret != ZMK_BEHAVIOR_OPAQUE) {
        return ret;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_toucan_caps_word_driver_api = {
    .binding_pressed = on_caps_word_binding_pressed,
    .binding_released = on_caps_word_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

static int caps_word_keycode_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_toucan_caps_word, caps_word_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_toucan_caps_word, zmk_keycode_state_changed);

static bool caps_word_is_caps_includelist(const struct behavior_toucan_caps_word_config *config,
                                          uint16_t usage_page, uint8_t usage_id,
                                          uint8_t implicit_modifiers) {
    for (int i = 0; i < config->continuations_count; i++) {
        const struct caps_word_continue_item *continuation = &config->continuations[i];
        LOG_DBG("Comparing with 0x%02X - 0x%02X (with implicit mods: 0x%02X)",
                continuation->page, continuation->id, continuation->implicit_modifiers);

        if (continuation->page == usage_page && continuation->id == usage_id &&
            (continuation->implicit_modifiers &
             (implicit_modifiers | zmk_hid_get_explicit_mods())) ==
                continuation->implicit_modifiers) {
            LOG_DBG("Continuing capsword, found included usage: 0x%02X - 0x%02X", usage_page,
                    usage_id);
            return true;
        }
    }

    return false;
}

static bool caps_word_is_alpha(uint8_t usage_id) {
    return (usage_id >= HID_USAGE_KEY_KEYBOARD_A && usage_id <= HID_USAGE_KEY_KEYBOARD_Z);
}

static bool caps_word_is_numeric(uint8_t usage_id) {
    return (usage_id >= HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION &&
            usage_id <= HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS);
}

static int caps_word_keycode_state_changed_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (int i = 0; i < ARRAY_SIZE(devs); i++) {
        const struct device *dev = devs[i];

        struct behavior_toucan_caps_word_data *data = dev->data;
        if (!data->active) {
            continue;
        }

        const struct behavior_toucan_caps_word_config *config = dev->config;

        if (!caps_word_is_alpha(ev->keycode) && !caps_word_is_numeric(ev->keycode) &&
            !is_mod(ev->usage_page, ev->keycode) &&
            !caps_word_is_caps_includelist(config, ev->usage_page, ev->keycode,
                                           ev->implicit_modifiers)) {
            LOG_DBG("Deactivating caps_word for 0x%02X - 0x%02X", ev->usage_page, ev->keycode);
            deactivate_caps_word(dev);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

#define PARSE_CONTINUATION(i)                                                                      \
    {.page = ZMK_HID_USAGE_PAGE(i), .id = ZMK_HID_USAGE_ID(i), .implicit_modifiers = SELECT_MODS(i)}

#define CONTINUATION_ITEM(i, n) PARSE_CONTINUATION(DT_INST_PROP_BY_IDX(n, continue_list, i))

#define TOUCAN_CAPS_WORD_INST(n)                                                                   \
    static struct behavior_toucan_caps_word_data behavior_toucan_caps_word_data_##n = {            \
        .active = false};                                                                          \
    static const struct behavior_toucan_caps_word_config behavior_toucan_caps_word_config_##n = {   \
        .continuations = {                                                                         \
            LISTIFY(DT_INST_PROP_LEN(n, continue_list), CONTINUATION_ITEM, (, ), n)},              \
        .continuations_count = DT_INST_PROP_LEN(n, continue_list),                                 \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_toucan_caps_word_data_##n,                    \
                            &behavior_toucan_caps_word_config_##n, POST_KERNEL,                    \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                    \
                            &behavior_toucan_caps_word_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TOUCAN_CAPS_WORD_INST)

#endif
