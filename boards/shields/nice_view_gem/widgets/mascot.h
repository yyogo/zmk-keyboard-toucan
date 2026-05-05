#pragma once

#include <lvgl.h>
#include "util.h"

void mascot_widget_init(void);
void draw_mascot(lv_obj_t *canvas, const struct status_state *state);
