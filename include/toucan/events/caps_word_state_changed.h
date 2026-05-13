#pragma once

#include <stdbool.h>
#include <zmk/event_manager.h>

struct toucan_caps_word_state_changed {
    bool active;
};

ZMK_EVENT_DECLARE(toucan_caps_word_state_changed);
