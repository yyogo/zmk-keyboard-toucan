#pragma once

#include <lvgl.h>
#include "util.h"

bool is_boot_logo_active(void);
void start_boot_logo(void);
void draw_boot_logo(lv_obj_t *canvas);
