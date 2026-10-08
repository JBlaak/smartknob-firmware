#include "speaker.h"

static const lv_coord_t BAR_HEIGHT = 12;

static void bar_anim_cb(void *var, int32_t value)
{
    lv_obj_set_height((lv_obj_t *)var, value);
}

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
    tint = SK_TINT_SPEAKER;
    glyph = &glyph_speaker_22;

    initScreen();
}

void SpeakerApp::initScreen()
{
    SemaphoreGuard lock(mutex_);
    lv_obj_set_style_bg_color(screen, SK_COLOR_BACKGROUND, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    arc_ = new SkRimArc(screen, SK_TINT_SPEAKER);

    lv_obj_t *art = sk_circle_create(screen, 56, LV_COLOR_MAKE(0x22, 0x1E, 0x3A));
    lv_obj_align(art, LV_ALIGN_CENTER, 0, -52);
    lv_obj_t *note = sk_glyph_create(art, &glyph_music_24, SK_TINT_SPEAKER);
    lv_obj_center(note);

    track_label_ = sk_label_create(screen, &figtree_600_17, SK_COLOR_TEXT);
    lv_obj_set_width(track_label_, 150);
    lv_obj_set_style_text_align(track_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(track_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(track_label_, friendly_name);
    lv_obj_align(track_label_, LV_ALIGN_CENTER, 0, -4);

    artist_label_ = sk_label_create(screen, &figtree_500_13, SK_COLOR_TEXT_SECONDARY);
    lv_obj_set_width(artist_label_, 140);
    lv_obj_set_style_text_align(artist_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(artist_label_, LV_LABEL_LONG_DOT);
    lv_obj_align(artist_label_, LV_ALIGN_CENTER, 0, 18);

    // The status row: moving bars or a pause sign, then the volume.
    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, 16);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 44);

    bars_ = lv_obj_create(row);
    lv_obj_remove_style_all(bars_);
    lv_obj_set_size(bars_, 13, BAR_HEIGHT);
    for (uint8_t i = 0; i < 3; i++)
    {
        bar_[i] = lv_obj_create(bars_);
        lv_obj_remove_style_all(bar_[i]);
        lv_obj_set_size(bar_[i], 3, BAR_HEIGHT);
        lv_obj_set_style_radius(bar_[i], 1, 0);
        lv_obj_set_style_bg_opa(bar_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(bar_[i], SK_TINT_SPEAKER, 0);
        lv_obj_align(bar_[i], LV_ALIGN_BOTTOM_LEFT, i * 5, 0);
    }

    pause_ = lv_obj_create(row);
    lv_obj_remove_style_all(pause_);
    lv_obj_set_size(pause_, 10, 10);
    for (uint8_t i = 0; i < 2; i++)
    {
        lv_obj_t *stroke = lv_obj_create(pause_);
        lv_obj_remove_style_all(stroke);
        lv_obj_set_size(stroke, 3, 10);
        lv_obj_set_style_radius(stroke, 1, 0);
        lv_obj_set_style_bg_opa(stroke, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(stroke, SK_COLOR_TEXT_SECONDARY, 0);
        lv_obj_set_pos(stroke, i * 6, 0);
    }

    volume_label_ = sk_label_create(row, &figtree_600_17, SK_COLOR_TEXT);

    render();
}

// Callers hold mutex_.
void SpeakerApp::setPlayingAnimation(bool playing)
{
    if (playing == animating_)
    {
        return;
    }
    animating_ = playing;
    for (uint8_t i = 0; i < 3; i++)
    {
        lv_anim_del(bar_[i], bar_anim_cb);
        if (!playing)
        {
            lv_obj_set_height(bar_[i], BAR_HEIGHT);
            continue;
        }
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, bar_[i]);
        lv_anim_set_exec_cb(&a, bar_anim_cb);
        lv_anim_set_values(&a, 3, BAR_HEIGHT);
        lv_anim_set_time(&a, 340 + i * 90);
        lv_anim_set_playback_time(&a, 340 + i * 90);
        lv_anim_set_delay(&a, i * 140);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }
}

// Callers hold mutex_.
void SpeakerApp::render()
{
    arc_->setValue(volume_);
    lv_label_set_text_fmt(volume_label_, "%d%%", volume_);
    if (playing_)
    {
        lv_obj_clear_flag(bars_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pause_, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(bars_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pause_, LV_OBJ_FLAG_HIDDEN);
    }
    setPlayingAnimation(playing_);
}

std::string SpeakerApp::statusText()
{
    if (!playing_)
    {
        return "Paused";
    }
    return "Playing · " + std::to_string(volume_) + "%";
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
        // Nothing playing shows the speaker's name instead.
        has_track_ = strlen(track->valuestring) > 0;
        lv_label_set_text(track_label_, has_track_ ? track->valuestring : friendly_name);
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
    {
        SemaphoreGuard lock(mutex_);
        render();
    }
    return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
}
