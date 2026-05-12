#include <zephyr/kernel.h>
#include "output.h"

LV_IMG_DECLARE(bt);
LV_IMG_DECLARE(usb);

// Output strip lives in the bottom-left region of the screen, alongside
// the profile slots which sit at x=85. BT (7x11) anchors the left of
// the strip; USB (22x9) anchors the right. Only the active transport's
// icon is drawn -- icon position alone communicates which mode we're on.
#define BT_X   70
#define BT_Y   141
#define USB_X  12
#define USB_Y  142

static void draw_icon(lv_obj_t *canvas, int x, int y, const lv_img_dsc_t *img) {
    lv_draw_img_dsc_t dsc;
    lv_draw_img_dsc_init(&dsc);
    lv_canvas_draw_img(canvas, x, y, img, &dsc);
}

void draw_output_status(lv_obj_t *canvas, const struct status_state *state) {
    // always draw BT icon
    draw_icon(canvas, BT_X, BT_Y, &bt);
    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        draw_icon(canvas, USB_X, USB_Y, &usb);
        break;
    // case ZMK_TRANSPORT_BLE:
    //     draw_icon(canvas, BT_X, BT_Y, &bt);
    //     break;
    default:
        break;
    }
}
