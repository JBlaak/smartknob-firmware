#pragma once

// The look of the UI: a black screen, one bright tint per app, white numbers in
// Figtree and grey status text. The design lives at
// https://claude.ai/artifact/CLJP7zJptFhUWxeZNsnzgV (canvas "SmartKnob UI").

#include <lvgl.h>

#include "assets/images/glyphs/glyphs.h"

// Figtree, by weight and pixel size. The 38 px and larger ones only carry
// digits and the few characters the screens show next to them; see
// assets/fonts/Figtree/convert.sh.
LV_FONT_DECLARE(figtree_500_13);
LV_FONT_DECLARE(figtree_600_12);
LV_FONT_DECLARE(figtree_600_17);
LV_FONT_DECLARE(figtree_600_23);
LV_FONT_DECLARE(figtree_600_38);
LV_FONT_DECLARE(figtree_600_54);
LV_FONT_DECLARE(figtree_600_64);
LV_FONT_DECLARE(figtree_600_76);

const lv_color_t SK_COLOR_BACKGROUND = LV_COLOR_MAKE(0x00, 0x00, 0x00);
const lv_color_t SK_COLOR_SURFACE = LV_COLOR_MAKE(0x1C, 0x1C, 0x1E); // unfocused icons, the picked row
const lv_color_t SK_COLOR_TRACK = LV_COLOR_MAKE(0x2C, 0x2C, 0x2E);   // empty part of an arc, unlit ticks
const lv_color_t SK_COLOR_OUTLINE = LV_COLOR_MAKE(0x3A, 0x3A, 0x3C);
const lv_color_t SK_COLOR_TEXT = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF);
const lv_color_t SK_COLOR_TEXT_SECONDARY = LV_COLOR_MAKE(0x8E, 0x8E, 0x93);
const lv_color_t SK_COLOR_TEXT_DISABLED = LV_COLOR_MAKE(0x5A, 0x5A, 0x5E);

// One tint per kind of app.
const lv_color_t SK_TINT_LIGHTS = LV_COLOR_MAKE(0xFF, 0xC5, 0x42);
const lv_color_t SK_TINT_CLIMATE = LV_COLOR_MAKE(0xFF, 0x7A, 0x45);
const lv_color_t SK_TINT_SPEAKER = LV_COLOR_MAKE(0x8C, 0x7C, 0xFF);
const lv_color_t SK_TINT_BLINDS = LV_COLOR_MAKE(0x38, 0xC6, 0xD9);
const lv_color_t SK_TINT_SETTINGS = LV_COLOR_MAKE(0xA1, 0xA1, 0xA6);
const lv_color_t SK_TINT_SWITCH = LV_COLOR_MAKE(0x5B, 0xD1, 0x7C);
const lv_color_t SK_TINT_STOPWATCH = LV_COLOR_MAKE(0xFF, 0x9F, 0x0A);
const lv_color_t SK_TINT_MUSIC = LV_COLOR_MAKE(0x1E, 0xD7, 0x60);

// Motion. The ring and lists move on a spring that overshoots a little; things
// that follow the knob directly don't animate at all.
const uint32_t SK_ANIM_SPRING_MS = 520;
const uint32_t SK_ANIM_FADE_MS = 240;
const uint32_t SK_ANIM_OPEN_MS = 300;

// An lv_anim path that overshoots and settles, like CSS cubic-bezier(.34, 1.45, .5, 1).
int32_t sk_anim_path_spring(const lv_anim_t *a);

// A label in the theme's type, with no background or padding.
lv_obj_t *sk_label_create(lv_obj_t *parent, const lv_font_t *font, lv_color_t color);

// A filled circle of the given diameter.
lv_obj_t *sk_circle_create(lv_obj_t *parent, lv_coord_t diameter, lv_color_t color);

// An alpha image (the glyphs) drawn in a tint.
lv_obj_t *sk_glyph_create(lv_obj_t *parent, const lv_img_dsc_t *glyph, lv_color_t color);

// The 270° arc around the rim used by the dial screens, with a white knob at
// the end of the value. Value is 0-100.
class SkRimArc
{
public:
    SkRimArc(lv_obj_t *parent, lv_color_t tint);
    void setValue(int16_t value);
    void setTint(lv_color_t tint);

private:
    lv_obj_t *arc_;
    lv_obj_t *thumb_;
    bool indicator_visible_ = true;
};
