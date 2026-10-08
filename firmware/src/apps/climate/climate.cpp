#include "climate.h"

static const lv_color_t SK_TINT_COOLING = LV_COLOR_MAKE(0x5A, 0xC8, 0xFA);

static const lv_coord_t TICK_OUTER = 114;
static const lv_coord_t TICK_LENGTH = 12;
static const lv_coord_t TARGET_LENGTH = 22;

// Degrees clockwise from the top, -135 to 135 across the range.
static float tempAngle(float temp, int16_t min_temp, int16_t max_temp)
{
    return -135 + 270 * (temp - min_temp) / (max_temp - min_temp);
}

static void radialLine(lv_point_t points[2], float angle_deg, lv_coord_t outer, lv_coord_t length)
{
    float a = angle_deg * PI / 180;
    points[0].x = 120 + outer * sinf(a);
    points[0].y = 120 - outer * cosf(a);
    points[1].x = 120 + (outer - length) * sinf(a);
    points[1].y = 120 - (outer - length) * cosf(a);
}

ClimateApp::ClimateApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_) : App(mutex)
{
    sprintf(app_id, "%s", app_id_);
    sprintf(friendly_name, "%s", friendly_name_);
    sprintf(entity_id, "%s", entity_id_);

    motor_config = PB_SmartKnobConfig{
        target_,
        0,
        (uint8_t)target_,
        MIN_TEMP,
        MAX_TEMP,
        8.225806452 * PI / 120,
        2,
        1,
        1.1,
        "",
        0,
        {},
        0,
        27,
    };
    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);

    LV_IMG_DECLARE(x80_thermostat);
    LV_IMG_DECLARE(x40_thermostat);
    big_icon = x80_thermostat;
    small_icon = x40_thermostat;
    tint = SK_TINT_CLIMATE;
    glyph = &glyph_climate_22;

    initScreen();
}

void ClimateApp::initScreen()
{
    SemaphoreGuard lock(mutex_);
    lv_obj_set_style_bg_color(screen, SK_COLOR_BACKGROUND, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0; i < TICK_COUNT; i++)
    {
        radialLine(tick_points_[i], -135 + i * 6, TICK_OUTER, TICK_LENGTH);
        ticks_[i] = lv_line_create(screen);
        lv_line_set_points(ticks_[i], tick_points_[i], 2);
        lv_obj_set_style_line_width(ticks_[i], 2, 0);
        lv_obj_set_style_line_rounded(ticks_[i], true, 0);
        tick_colors_[i] = SK_COLOR_TRACK;
        lv_obj_set_style_line_color(ticks_[i], SK_COLOR_TRACK, 0);
    }

    target_tick_ = lv_line_create(screen);
    lv_obj_set_style_line_width(target_tick_, 3, 0);
    lv_obj_set_style_line_rounded(target_tick_, true, 0);
    lv_obj_set_style_line_color(target_tick_, SK_COLOR_TEXT, 0);

    mode_label_ = sk_label_create(screen, &figtree_600_12, SK_TINT_CLIMATE);
    lv_obj_set_style_text_letter_space(mode_label_, 1, 0);

    target_label_ = sk_label_create(screen, &figtree_600_76, SK_COLOR_TEXT);
    lv_obj_set_style_text_letter_space(target_label_, -3, 0);

    current_label_ = sk_label_create(screen, &figtree_500_13, SK_COLOR_TEXT_SECONDARY);

    render();
}

bool ClimateApp::heating()
{
    return has_current_ && mode_ != CLIMATE_OFF && mode_ != CLIMATE_COOL && target_ > current_;
}

bool ClimateApp::cooling()
{
    return has_current_ && (mode_ == CLIMATE_COOL || mode_ == CLIMATE_HEAT_COOL || mode_ == CLIMATE_AUTO) && target_ < current_;
}

// Callers hold mutex_.
void ClimateApp::render()
{
    float low = has_current_ ? LV_MIN(current_, (float)target_) : target_;
    float high = has_current_ ? LV_MAX(current_, (float)target_) : target_;
    lv_color_t lit = heating() ? SK_TINT_CLIMATE : cooling() ? SK_TINT_COOLING : SK_COLOR_TEXT_DISABLED;

    int8_t current_tick = has_current_ ? round((current_ - MIN_TEMP) * (TICK_COUNT - 1) / (MAX_TEMP - MIN_TEMP)) : -1;
    for (uint8_t i = 0; i < TICK_COUNT; i++)
    {
        float temp = MIN_TEMP + (float)(MAX_TEMP - MIN_TEMP) * i / (TICK_COUNT - 1);
        lv_color_t color = SK_COLOR_TRACK;
        if (temp >= low - 0.01 && temp <= high + 0.01 && has_current_)
        {
            color = lit;
        }
        if (i == current_tick)
        {
            color = SK_COLOR_TEXT_SECONDARY;
        }
        // Only restyle the ticks that change, so a detent redraws a few.
        if (color.full != tick_colors_[i].full)
        {
            tick_colors_[i] = color;
            lv_obj_set_style_line_color(ticks_[i], color, 0);
        }
    }

    radialLine(target_points_, tempAngle(target_, MIN_TEMP, MAX_TEMP), TICK_OUTER, TARGET_LENGTH);
    lv_line_set_points(target_tick_, target_points_, 2);

    if (mode_ == CLIMATE_OFF && has_mode_)
    {
        lv_label_set_text(mode_label_, "OFF");
        lv_obj_set_style_text_color(mode_label_, SK_COLOR_TEXT_SECONDARY, 0);
    }
    else if (heating())
    {
        lv_label_set_text(mode_label_, "HEATING");
        lv_obj_set_style_text_color(mode_label_, SK_TINT_CLIMATE, 0);
    }
    else if (cooling())
    {
        lv_label_set_text(mode_label_, "COOLING");
        lv_obj_set_style_text_color(mode_label_, SK_TINT_COOLING, 0);
    }
    else
    {
        lv_label_set_text(mode_label_, "IDLE");
        lv_obj_set_style_text_color(mode_label_, SK_COLOR_TEXT_SECONDARY, 0);
    }
    lv_obj_align(mode_label_, LV_ALIGN_CENTER, 0, -50);

    lv_label_set_text_fmt(target_label_, "%d°", target_);
    // Nudged right so the number, not number and degree sign, sits centred.
    lv_obj_align(target_label_, LV_ALIGN_CENTER, 10, -2);

    if (has_current_)
    {
        char current[16];
        if (current_ == roundf(current_))
        {
            snprintf(current, sizeof(current), "Inside %d°", (int)current_);
        }
        else
        {
            snprintf(current, sizeof(current), "Inside %.1f°", current_);
        }
        lv_label_set_text(current_label_, current);
    }
    else
    {
        lv_label_set_text(current_label_, "");
    }
    lv_obj_align(current_label_, LV_ALIGN_CENTER, 0, 44);
}

std::string ClimateApp::statusText()
{
    char status[32];
    if (has_mode_ && mode_ == CLIMATE_OFF)
    {
        return "Off";
    }
    if (heating())
    {
        snprintf(status, sizeof(status), "Heating to %d°", target_);
    }
    else if (cooling())
    {
        snprintf(status, sizeof(status), "Cooling to %d°", target_);
    }
    else
    {
        snprintf(status, sizeof(status), "Set to %d°", target_);
    }
    return status;
}

EntityStateUpdate ClimateApp::updateStateFromKnob(PB_SmartKnobState state)
{
    if (state_sent_from_hass)
    {
        state_sent_from_hass = false;
        return EntityStateUpdate();
    }

    target_ = constrain(state.current_position, MIN_TEMP, MAX_TEMP);
    motor_config.position = target_;
    motor_config.position_nonce = target_;
    if (target_ == last_target_)
    {
        return EntityStateUpdate();
    }
    last_target_ = target_;

    {
        SemaphoreGuard lock(mutex_);
        render();
    }

    EntityStateUpdate new_state;
    snprintf(new_state.app_id, sizeof(new_state.app_id), "%s", app_id);
    snprintf(new_state.entity_id, sizeof(new_state.entity_id), "%s", entity_id);
    snprintf(new_state.app_slug, sizeof(new_state.app_slug), "%s", APP_SLUG_CLIMATE);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "target_temp", target_);
    if (has_current_)
    {
        cJSON_AddNumberToObject(json, "current_temp", current_);
    }
    if (has_mode_)
    {
        cJSON_AddNumberToObject(json, "mode", mode_);
    }
    char *json_str = cJSON_PrintUnformatted(json);
    snprintf(new_state.state, sizeof(new_state.state), "%s", json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);

    new_state.changed = true;
    return new_state;
}

void ClimateApp::updateStateFromHASS(MQTTStateUpdate mqtt_state_update)
{
    cJSON *new_state = cJSON_Parse(mqtt_state_update.state);
    if (new_state == NULL)
    {
        LOGW("Invalid climate state");
        return;
    }
    cJSON *mode = cJSON_GetObjectItem(new_state, "mode");
    cJSON *target_temp = cJSON_GetObjectItem(new_state, "target_temp");
    cJSON *current_temp = cJSON_GetObjectItem(new_state, "current_temp");

    if (cJSON_IsNumber(mode) && mode->valueint >= 0 && mode->valueint < CLIMATE_MODE_COUNT)
    {
        mode_ = static_cast<ClimateAppMode>(mode->valueint);
        has_mode_ = true;
    }
    if (cJSON_IsNumber(current_temp))
    {
        current_ = current_temp->valuedouble;
        has_current_ = true;
    }
    if (cJSON_IsNumber(target_temp))
    {
        int16_t target = constrain((int16_t)round(target_temp->valuedouble), MIN_TEMP, MAX_TEMP);
        if (target != target_)
        {
            target_ = target;
            last_target_ = target;
            motor_config.position = target_;
            motor_config.position_nonce = target_;
            state_sent_from_hass = true;
        }
    }
    cJSON_Delete(new_state);

    SemaphoreGuard lock(mutex_);
    render();
}

int8_t ClimateApp::navigationNext()
{
    return DONT_NAVIGATE;
}
