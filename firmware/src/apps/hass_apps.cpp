#include "hass_apps.h"

static void breathe_anim_cb(void *var, int32_t value)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)var, value, 0);
}

HassApps::HassApps(SemaphoreHandle_t mutex) : Apps(mutex)
{
    lv_obj_set_style_bg_color(waiting_for_hass, SK_COLOR_BACKGROUND, 0);
    lv_obj_clear_flag(waiting_for_hass, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dot = sk_circle_create(waiting_for_hass, 14, SK_COLOR_TEXT);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, -52);

    lv_obj_t *title = sk_label_create(waiting_for_hass, &figtree_600_23, SK_COLOR_TEXT);
    lv_label_set_text(title, "Knocking on\nthe door…");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -4);

    lv_obj_t *subtitle = sk_label_create(waiting_for_hass, &figtree_500_13, SK_COLOR_TEXT_SECONDARY);
    lv_label_set_text(subtitle, "Waiting for your controller");
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 42);

    // The dot breathes while the knob waits.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, dot);
    lv_anim_set_exec_cb(&a, breathe_anim_cb);
    lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
    lv_anim_set_time(&a, 1200);
    lv_anim_set_playback_time(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
};
void HassApps::sync(cJSON *json_apps)
{
    clear();
    uint16_t app_position = 0;

    cJSON *json_app_ = NULL;
    cJSON_ArrayForEach(json_app_, json_apps)
    {

        cJSON *json_app_slug = cJSON_GetObjectItemCaseSensitive(json_app_, "app_slug");
        cJSON *json_app_id = cJSON_GetObjectItemCaseSensitive(json_app_, "app_id");
        cJSON *json_entity_id = cJSON_GetObjectItemCaseSensitive(json_app_, "entity_id");
        cJSON *json_friendly_name = cJSON_GetObjectItemCaseSensitive(json_app_, "friendly_name");

        if (json_app_slug == NULL || json_app_id == NULL || json_entity_id == NULL || json_friendly_name == NULL)
        {
            LOGE("Invalid data.");
            continue;
        }

        loadApp(app_position, json_app_slug->valuestring, json_app_id->valuestring, json_friendly_name->valuestring, json_entity_id->valuestring);

        app_position++;
    }

    SettingsApp *settings_app = new SettingsApp(screen_mutex_);
    settings_app->setOSConfigNotifier(os_config_notifier_);
    add(app_position, settings_app);

    updateMenu();
    setMotorNotifier(motor_notifier);
    cJSON_Delete(json_apps); // DELETING DELETES POINTERS NEEDED TO DISPLAY FRIENDLY NAME ON APPS HMMMM
}
void HassApps::handleEvent(WiFiEvent event)
{
    SemaphoreGuard lock(app_mutex_);
    std::shared_ptr<App> app;

    switch (event.type)
    {
    case SK_MQTT_STATE_UPDATE:
        if (event.body.mqtt_state_update.all == true)
        {
            for (auto &app : apps)
            {

                if (strcmp(app.second->app_id, event.body.mqtt_state_update.app_id) != 0 && strcmp(app.second->entity_id, event.body.mqtt_state_update.entity_id) == 0)
                {
                    app.second->updateStateFromHASS(event.body.mqtt_state_update);
                }
            }
        }
        else
        {
            app = find(event.body.mqtt_state_update.app_id);
            if (app != nullptr)
            {
                app->updateStateFromHASS(event.body.mqtt_state_update);
                // Only the app on screen drives the motor. Sending another
                // app's update would hand the motor the active app's config
                // as it was when queued, snapping the dial back mid-turn.
                if (app == active_app)
                {
                    motor_notifier->requestUpdate(active_app->getMotorConfig());
                }
                if (menu != nullptr)
                {
                    menu->refreshStatus();
                }
            }
            else
            {
                LOGW("App not found");
            }
        }
        break;
    default:
        break;
    }
}
void HassApps::handleNavigationEvent(NavigationEvent event)
{
    if (active_app == nullptr || apps.size() <= 1) // 1 is menu which doesnt get removed when sync = 0 apps
    {
        return;
    }
    return Apps::handleNavigationEvent(event);
}

void HassApps::render()
{
    if (active_app == nullptr || apps.size() <= 1) // 1 is menu which doesnt get removed when sync = 0 apps
    {
        return lv_scr_load(waiting_for_hass);
    }
    return Apps::render();
}