#include "speaker.h"

SpeakerApp::SpeakerApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_) : App(mutex)
{
    sprintf(app_id, "%s", app_id_);
    sprintf(friendly_name, "%s", friendly_name_);
    sprintf(entity_id, "%s", entity_id_);

    motor_config = PB_SmartKnobConfig{
        volume_,
        0,
        volume_,
        0,
        100,
        2.4 * PI / 180,
        1,
        1,
        1.1,
        "",
        0,
        {},
        0,
        27,
    };
    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);

    LV_IMG_DECLARE(x80_speaker);
    LV_IMG_DECLARE(x40_speaker);

    big_icon = x80_speaker;
    small_icon = x40_speaker;

    initScreen();
}

void SpeakerApp::initScreen()
{
    SemaphoreGuard lock(mutex_);

    arc_ = lv_arc_create(screen);
    lv_obj_set_size(arc_, 220, 220);
    lv_arc_set_rotation(arc_, 150);
    lv_arc_set_bg_angles(arc_, 0, 240);
    lv_arc_set_range(arc_, 0, 100);
    lv_arc_set_value(arc_, volume_);
    lv_obj_remove_style(arc_, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc_, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_, dark_arc_bg, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_, LV_COLOR_MAKE(0xF8, 0xCA, 0x05), LV_PART_INDICATOR);
    lv_obj_center(arc_);

    lv_obj_t *name_label = lv_label_create(screen);
    lv_label_set_text(name_label, friendly_name);
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, -62);

    volume_label_ = lv_label_create(screen);
    lv_obj_set_style_text_font(volume_label_, &roboto_light_mono_24pt, 0);
    lv_obj_align(volume_label_, LV_ALIGN_CENTER, 0, -26);

    playing_label_ = lv_label_create(screen);
    lv_obj_set_style_text_color(playing_label_, LV_COLOR_MAKE(0x99, 0x99, 0x99), 0);
    lv_obj_align(playing_label_, LV_ALIGN_CENTER, 0, 8);

    track_label_ = lv_label_create(screen);
    lv_label_set_long_mode(track_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(track_label_, 150);
    lv_obj_set_style_text_align(track_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(track_label_, "");
    lv_obj_align(track_label_, LV_ALIGN_CENTER, 0, 38);

    artist_label_ = lv_label_create(screen);
    lv_label_set_long_mode(artist_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_width(artist_label_, 130);
    lv_obj_set_style_text_align(artist_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(artist_label_, LV_COLOR_MAKE(0x99, 0x99, 0x99), 0);
    lv_label_set_text(artist_label_, "");
    lv_obj_align(artist_label_, LV_ALIGN_CENTER, 0, 62);

    render();
}

// Callers hold mutex_.
void SpeakerApp::render()
{
    lv_arc_set_value(arc_, volume_);
    lv_label_set_text_fmt(volume_label_, "%d%%", volume_);
    lv_obj_align(volume_label_, LV_ALIGN_CENTER, 0, -26);
    lv_label_set_text(playing_label_, playing_ ? "PLAYING" : "PAUSED");
    lv_obj_align(playing_label_, LV_ALIGN_CENTER, 0, 8);
}

EntityStateUpdate SpeakerApp::stateUpdate()
{
    EntityStateUpdate new_state;
    sprintf(new_state.app_id, "%s", app_id);
    sprintf(new_state.entity_id, "%s", entity_id);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "volume", volume_);
    cJSON_AddBoolToObject(json, "playing", playing_);
    char *json_str = cJSON_PrintUnformatted(json);
    snprintf(new_state.state, sizeof(new_state.state), "%s", json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);

    new_state.changed = true;
    sprintf(new_state.app_slug, "%s", APP_SLUG_SPEAKER);
    return new_state;
}

EntityStateUpdate SpeakerApp::updateStateFromKnob(PB_SmartKnobState state)
{
    if (state_sent_from_hass)
    {
        state_sent_from_hass = false;
        return EntityStateUpdate{};
    }

    volume_ = state.current_position;
    motor_config.position = volume_;
    motor_config.position_nonce = volume_;

    if (volume_ == last_volume_ && !playing_toggled_)
    {
        return EntityStateUpdate{};
    }

    {
        SemaphoreGuard lock(mutex_);
        render();
    }

    last_volume_ = volume_;
    playing_toggled_ = false;
    return stateUpdate();
}

void SpeakerApp::updateStateFromHASS(MQTTStateUpdate mqtt_state_update)
{
    cJSON *new_state = cJSON_Parse(mqtt_state_update.state);
    if (new_state == NULL)
    {
        LOGW("Invalid speaker state");
        return;
    }

    cJSON *volume = cJSON_GetObjectItem(new_state, "volume");
    cJSON *playing = cJSON_GetObjectItem(new_state, "playing");
    cJSON *track = cJSON_GetObjectItem(new_state, "track");
    cJSON *artist = cJSON_GetObjectItem(new_state, "artist");

    SemaphoreGuard lock(mutex_);

    if (cJSON_IsNumber(volume))
    {
        volume_ = constrain(volume->valueint, 0, 100);
        last_volume_ = volume_;
        motor_config.position = volume_;
        motor_config.position_nonce = volume_;
        state_sent_from_hass = true;
    }
    if (cJSON_IsBool(playing))
    {
        playing_ = cJSON_IsTrue(playing);
    }
    if (cJSON_IsString(track))
    {
        lv_label_set_text(track_label_, track->valuestring);
    }
    if (cJSON_IsString(artist))
    {
        lv_label_set_text(artist_label_, artist->valuestring);
    }

    render();
    cJSON_Delete(new_state);
}

// Pressing plays or pauses. The new state goes out with the next knob update,
// and the motor config is left as it is so the dial doesn't move.
int8_t SpeakerApp::navigationNext()
{
    playing_ = !playing_;
    playing_toggled_ = true;
    return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
}
