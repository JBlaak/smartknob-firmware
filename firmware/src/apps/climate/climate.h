#pragma once
#include "../app.h"

enum ClimateAppMode : uint8_t
{
    CLIMATE_OFF = 0,
    CLIMATE_HEAT,
    CLIMATE_COOL,
    CLIMATE_HEAT_COOL,
    CLIMATE_AUTO,
    CLIMATE_DRY,
    CLIMATE_FAN_ONLY,
    CLIMATE_MODE_COUNT
};

// A thermostat. Ticks run around the rim; turning moves the target in whole
// degrees, and the ticks between the inside temperature and the target light
// up while it heats (or cools).
//
// State to the controller:   {"target_temp": int, "current_temp": number, "mode": int}
// State from the controller: the same, all optional. The mode is only sent back
// once the controller has sent one, so the knob never changes it by itself.
class ClimateApp : public App
{
public:
    ClimateApp(SemaphoreHandle_t mutex, char *app_id, char *friendly_name, char *entity_id);

    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;
    void updateStateFromHASS(MQTTStateUpdate mqtt_state_update) override;
    int8_t navigationNext() override;
    std::string statusText() override;

private:
    static const int16_t MIN_TEMP = 10;
    static const int16_t MAX_TEMP = 30;
    static const uint8_t TICK_COUNT = 46; // one every 6°

    void initScreen();
    void render();
    bool heating();
    bool cooling();

    lv_obj_t *ticks_[TICK_COUNT];
    lv_point_t tick_points_[TICK_COUNT][2];
    lv_color_t tick_colors_[TICK_COUNT];
    lv_obj_t *target_tick_;
    lv_point_t target_points_[2];
    lv_obj_t *mode_label_;
    lv_obj_t *target_label_;
    lv_obj_t *current_label_;

    int16_t target_ = 20;
    int16_t last_target_ = 20;
    float current_ = 20;
    bool has_current_ = false;
    ClimateAppMode mode_ = CLIMATE_AUTO;
    bool has_mode_ = false;
};
