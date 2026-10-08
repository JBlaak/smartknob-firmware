#include "light_dimmer.h"
#include "cJSON.h"

// The glow is a soft radial image whose opacity follows the brightness. It
// changes in steps, since redrawing it is a full-screen redraw.
static const int8_t GLOW_STEPS = 8;
static const lv_opa_t GLOW_MAX_OPA = 120;

LightDimmerApp::LightDimmerApp(SemaphoreHandle_t mutex, AppData app_data) : App(mutex)
{
    strcpy(app_id, app_data.app_id);
    strcpy(friendly_name, app_data.friendly_name);
    strcpy(entity_id, app_data.entity_id);

    motor_config = PB_SmartKnobConfig{
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 0,
        .min_position = 0,
        .max_position = 100,
        .position_width_radians = 2.4 * PI / 180,
        .detent_strength_unit = 1,
        .endstop_strength_unit = 1,
        .snap_point = 1.1,
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
        .led_hue = 27,
    };
    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);

    LV_IMG_DECLARE(x80_light_outline);
    LV_IMG_DECLARE(x40_light_outline);
    big_icon = x80_light_outline;
    small_icon = x40_light_outline;
    tint = SK_TINT_LIGHTS;
    glyph = &glyph_lights_22;

    initScreen();
}

void LightDimmerApp::initScreen()
{
    SemaphoreGuard lock(mutex_);
    lv_obj_set_style_bg_color(screen, SK_COLOR_BACKGROUND, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    glow_ = sk_glyph_create(screen, &glow_240, SK_TINT_LIGHTS);
    lv_obj_center(glow_);

    arc_ = new SkRimArc(screen, SK_TINT_LIGHTS);

    bulb_ = sk_glyph_create(screen, &glyph_lights_24, SK_TINT_LIGHTS);
    lv_obj_align(bulb_, LV_ALIGN_CENTER, 0, -50);

    value_label_ = sk_label_create(screen, &figtree_600_64, SK_COLOR_TEXT);
    lv_obj_set_style_text_letter_space(value_label_, -2, 0);

    unit_label_ = sk_label_create(screen, &figtree_600_23, SK_COLOR_TEXT_SECONDARY);
    lv_label_set_text(unit_label_, "%");

    lv_obj_t *name_label = sk_label_create(screen, &figtree_500_13, SK_COLOR_TEXT_SECONDARY);
    lv_obj_set_width(name_label, 150);
    lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(name_label, friendly_name);
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, 40);

    render();
}

// Callers hold mutex_.
void LightDimmerApp::render()
{
    arc_->setValue(brightness_);

    int8_t glow_step = (brightness_ * GLOW_STEPS + 99) / 100;
    if (glow_step != glow_step_)
    {
        glow_step_ = glow_step;
        lv_obj_set_style_img_opa(glow_, GLOW_MAX_OPA * glow_step / GLOW_STEPS, 0);
        if (glow_step == 0)
        {
            lv_obj_add_flag(glow_, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(glow_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if ((brightness_ > 0) != (last_rendered_ > 0) || last_rendered_ < 0)
    {
        lv_obj_set_style_img_recolor(bulb_, brightness_ > 0 ? SK_TINT_LIGHTS : SK_COLOR_TEXT_DISABLED, 0);
    }
    last_rendered_ = brightness_;

    if (brightness_ > 0)
    {
        lv_label_set_text_fmt(value_label_, "%d", brightness_);
        lv_obj_clear_flag(unit_label_, LV_OBJ_FLAG_HIDDEN);
        // The number and its % sit centred together.
        lv_obj_update_layout(value_label_);
        lv_obj_update_layout(unit_label_);
        lv_coord_t value_width = lv_obj_get_width(value_label_);
        lv_coord_t unit_width = lv_obj_get_width(unit_label_);
        lv_coord_t left = -(value_width + 2 + unit_width) / 2;
        lv_obj_align(value_label_, LV_ALIGN_CENTER, left + value_width / 2, -2);
        lv_obj_align_to(unit_label_, value_label_, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -10);
    }
    else
    {
        lv_label_set_text(value_label_, "Off");
        lv_obj_add_flag(unit_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(value_label_, LV_ALIGN_CENTER, 0, -2);
    }
}

std::string LightDimmerApp::statusText()
{
    if (brightness_ == 0)
    {
        return "Off";
    }
    return std::to_string(brightness_) + "%";
}

void LightDimmerApp::setBrightness(int16_t brightness)
{
    brightness_ = constrain(brightness, 0, 100);
    if (brightness_ > 0)
    {
        on_brightness_ = brightness_;
    }
    motor_config.position = brightness_;
    motor_config.position_nonce++;
}

EntityStateUpdate LightDimmerApp::updateStateFromKnob(PB_SmartKnobState state)
{
    if (state_sent_from_hass)
    {
        state_sent_from_hass = false;
        return EntityStateUpdate();
    }

    brightness_ = constrain(state.current_position, 0, 100);
    motor_config.position = brightness_;
    if (brightness_ == last_brightness_)
    {
        return EntityStateUpdate();
    }
    last_brightness_ = brightness_;
    if (brightness_ > 0)
    {
        on_brightness_ = brightness_;
    }

    {
        SemaphoreGuard lock(mutex_);
        render();
    }

    EntityStateUpdate new_state;
    snprintf(new_state.app_id, sizeof(new_state.app_id), "%s", app_id);
    snprintf(new_state.entity_id, sizeof(new_state.entity_id), "%s", entity_id);
    snprintf(new_state.app_slug, sizeof(new_state.app_slug), "%s", APP_SLUG_LIGHT_DIMMER);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "on", brightness_ > 0);
    cJSON_AddNumberToObject(json, "brightness", round(brightness_ * 2.55));
    char *json_str = cJSON_PrintUnformatted(json);
    snprintf(new_state.state, sizeof(new_state.state), "%s", json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);

    new_state.changed = true;
    return new_state;
}

void LightDimmerApp::updateStateFromHASS(MQTTStateUpdate mqtt_state_update)
{
    cJSON *new_state = cJSON_Parse(mqtt_state_update.state);
    if (new_state == NULL)
    {
        LOGW("Invalid light state");
        return;
    }
    cJSON *on = cJSON_GetObjectItem(new_state, "on");
    cJSON *brightness = cJSON_GetObjectItem(new_state, "brightness");

    int16_t new_brightness = brightness_;
    if (cJSON_IsNumber(brightness))
    {
        new_brightness = round(brightness->valuedouble / 2.55);
    }
    if (cJSON_IsBool(on))
    {
        if (!cJSON_IsTrue(on))
        {
            new_brightness = 0;
        }
        else if (new_brightness == 0)
        {
            new_brightness = on_brightness_;
        }
    }
    cJSON_Delete(new_state);

    if (new_brightness == brightness_)
    {
        return;
    }
    setBrightness(new_brightness);
    last_brightness_ = brightness_;
    state_sent_from_hass = true;

    SemaphoreGuard lock(mutex_);
    render();
}

// Pressing switches off, or back on where it was. Moving the motor's position
// makes the next knob update send the new state.
int8_t LightDimmerApp::navigationNext()
{
    setBrightness(brightness_ > 0 ? 0 : on_brightness_);
    return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
}
