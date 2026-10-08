#pragma once
#include "../app.h"

// A dimmable light, or a group of them. Turning sets the brightness on a warm
// arc, and the screen glows brighter with it. Pressing switches off, or back
// on at the last brightness.
//
// State to the controller:   {"on": bool, "brightness": 0-255}
// State from the controller: {"on": bool, "brightness": 0-255} (both optional)
class LightDimmerApp : public App
{
public:
    LightDimmerApp(SemaphoreHandle_t mutex, AppData app_data);

    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;
    void updateStateFromHASS(MQTTStateUpdate mqtt_state_update) override;
    int8_t navigationNext() override;
    std::string statusText() override;

private:
    void initScreen();
    void render();
    void setBrightness(int16_t brightness);

    SkRimArc *arc_;
    lv_obj_t *glow_;
    lv_obj_t *bulb_;
    lv_obj_t *value_label_;
    lv_obj_t *unit_label_;

    int16_t brightness_ = 0;      // 0-100
    int16_t last_brightness_ = 0; // last brightness sent or received
    int16_t on_brightness_ = 100; // where pressing switches back on
    int8_t glow_step_ = -1;
    int16_t last_rendered_ = -1;
};
