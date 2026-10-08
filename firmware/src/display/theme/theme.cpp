#include "theme.h"

int32_t sk_anim_path_spring(const lv_anim_t *a)
{
    uint32_t t = lv_map(a->act_time, 0, a->time, 0, LV_BEZIER_VAL_MAX);
    int32_t step = lv_bezier3(t, 0, 1485, 1024, 1024);
    int32_t value = step * (a->end_value - a->start_value);
    value = value >> LV_BEZIER_VAL_SHIFT;
    return value + a->start_value;
}

lv_obj_t *sk_label_create(lv_obj_t *parent, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, "");
    return label;
}

lv_obj_t *sk_circle_create(lv_obj_t *parent, lv_coord_t diameter, lv_color_t color)
{
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, diameter, diameter);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(circle, color, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return circle;
}

lv_obj_t *sk_glyph_create(lv_obj_t *parent, const lv_img_dsc_t *glyph, lv_color_t color)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, glyph);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(img, color, 0);
    return img;
}

// The arc's stroke is centred on a 106 px radius, 12 px wide, leaving a gap of
// 90° at the bottom.
static const lv_coord_t RIM_RADIUS = 106;
static const lv_coord_t RIM_WIDTH = 12;
static const lv_coord_t THUMB_DIAMETER = 16;

SkRimArc::SkRimArc(lv_obj_t *parent, lv_color_t tint)
{
    arc_ = lv_arc_create(parent);
    lv_obj_set_size(arc_, (RIM_RADIUS + RIM_WIDTH / 2) * 2, (RIM_RADIUS + RIM_WIDTH / 2) * 2);
    lv_arc_set_rotation(arc_, 135);
    lv_arc_set_bg_angles(arc_, 0, 270);
    lv_arc_set_range(arc_, 0, 100);
    lv_obj_remove_style(arc_, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(arc_, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, RIM_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, RIM_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_, SK_COLOR_TRACK, LV_PART_MAIN);
    lv_obj_center(arc_);

    thumb_ = sk_circle_create(parent, THUMB_DIAMETER, SK_COLOR_TEXT);
    lv_obj_set_style_border_width(thumb_, 3, 0);
    lv_obj_set_style_border_color(thumb_, SK_COLOR_BACKGROUND, 0);
    lv_obj_set_style_border_post(thumb_, false, 0);

    setTint(tint);
    setValue(0);
}

void SkRimArc::setTint(lv_color_t tint)
{
    lv_obj_set_style_arc_color(arc_, tint, LV_PART_INDICATOR);
}

void SkRimArc::setValue(int16_t value)
{
    value = LV_CLAMP(0, value, 100);
    lv_arc_set_value(arc_, value);
    // A zero-length arc with rounded ends still draws a dot. Only touch the
    // style when that changes: setting it redraws the whole arc.
    bool visible = value > 0;
    if (visible != indicator_visible_)
    {
        indicator_visible_ = visible;
        lv_obj_set_style_arc_opa(arc_, visible ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
    }

    int16_t angle = 135 + 270 * value / 100;
    lv_coord_t x = LV_HOR_RES / 2 + (RIM_RADIUS * lv_trigo_cos(angle) >> LV_TRIGO_SHIFT);
    lv_coord_t y = LV_VER_RES / 2 + (RIM_RADIUS * lv_trigo_sin(angle) >> LV_TRIGO_SHIFT);
    lv_obj_set_pos(thumb_, x - THUMB_DIAMETER / 2, y - THUMB_DIAMETER / 2);
}
