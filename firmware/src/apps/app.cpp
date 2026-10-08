#include "app.h"

App::App(SemaphoreHandle_t mutex) : mutex_(mutex)
{
    lv_obj_set_style_bg_color(screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
}

App::App(SemaphoreHandle_t mutex, int8_t next, int8_t back) : mutex_(mutex), next_(next), back_(back)
{
    lv_obj_set_style_bg_color(screen, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_size(screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
}

void App::render()
{
    show(SCREEN_TRANSITION_NONE);
}

void App::show(ScreenTransition transition)
{
    SemaphoreGuard lock(mutex_);
    switch (transition)
    {
    case SCREEN_TRANSITION_OPEN:
        // Waits for the home screen to cover itself in this app's tint, then
        // fades in over it.
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, SK_ANIM_FADE_MS, SK_ANIM_OPEN_MS, false);
        break;
    case SCREEN_TRANSITION_BACK:
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, SK_ANIM_FADE_MS, 0, false);
        break;
    default:
        lv_scr_load(screen);
        break;
    }
}

void App::setMotorNotifier(MotorNotifier *motor_notifier)
{
    this->motor_notifier = motor_notifier;
}

void App::triggerMotorConfigUpdate()
{
    if (this->motor_notifier != nullptr)
    {
        motor_notifier->requestUpdate(root_level_motor_config);
    }
    else
    {
        LOGW("Motor_notifier is not set");
    }
}

void App::setNext(int8_t next)
{
    next_ = next;
}
void App::setBack(int8_t back)
{
    back_ = back;
}

PB_SmartKnobConfig App::getMotorConfig()
{
    return motor_config;
}

std::string App::getClassName()
{
    return "App";
}