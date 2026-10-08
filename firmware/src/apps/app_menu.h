#pragma once
#include "app.h"

#include <memory>
#include <vector>

// The home screen. The apps sit as icons on a ring around the rim; turning
// rolls the ring so the next one comes to the top, with its name and status in
// the middle. Pressing grows the top icon until it fills the screen and opens
// the app. Left alone, the ring folds into dots around a clock.
//
// All of the ring's motion comes from one animated value, ring_pos_ (the
// knob's position times 1000), so a turn mid-animation just retargets it.
class MenuApp : public App
{
public:
    MenuApp(SemaphoreHandle_t mutex);
    ~MenuApp();

    // Adds an app to the ring. id is its key in Apps.
    void addItem(int8_t id, std::shared_ptr<App> app);

    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;
    int8_t navigationNext() override;
    void show(ScreenTransition transition) override;

    // Re-reads every app's status line. Call after an app's state changed.
    void refreshStatus();

private:
    struct Item
    {
        int8_t id;
        std::shared_ptr<App> app;
        lv_obj_t *bubble;
        lv_obj_t *glyph;
        lv_obj_t *text;
        lv_obj_t *name;
        lv_obj_t *status;
    };

    void layout();
    void setRingPos(int32_t ring_pos);
    void setIdleAmount(int32_t idle_amount);
    void setOpenAmount(int32_t open_amount);
    void animateRingTo(int32_t ring_pos);
    void setIdle(bool idle);
    void updateClock();
    int focusedIndex();

    static void ringAnimCb(void *var, int32_t value);
    static void idleAnimCb(void *var, int32_t value);
    static void openAnimCb(void *var, int32_t value);
    static void idleTimerCb(lv_timer_t *timer);

    std::vector<Item> items_;

    lv_obj_t *rim_dot_;
    lv_obj_t *clock_;
    lv_obj_t *time_label_;
    lv_obj_t *date_label_;
    lv_obj_t *grow_;
    lv_timer_t *idle_timer_ = nullptr;

    int32_t position_ = 0;        // the knob's position
    int32_t position_offset_ = 0; // turns that only woke the screen
    int32_t ring_pos_ = 0;        // shown position x 1000, animated
    int32_t idle_amount_ = 0;     // 0 awake .. 1000 folded into the clock
    int32_t open_amount_ = 0;     // 0 .. 1000 grown to fill the screen
    bool idle_ = false;
    volatile uint32_t last_input_ms_ = 0;
    char shown_time_[8] = "";
};
