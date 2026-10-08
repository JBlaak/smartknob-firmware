#include "apps.h"

Apps::Apps(SemaphoreHandle_t mutex) : screen_mutex_(mutex)
{
    app_mutex_ = xSemaphoreCreateMutex();
}

void Apps::add(uint8_t id, App *app)
{
    SemaphoreGuard lock(app_mutex_);
    apps.insert(std::make_pair(id, app));
}

void Apps::clear()
{
    SemaphoreGuard lock(app_mutex_);
    apps.clear();
}

EntityStateUpdate Apps::update(AppState state)
{
    // TODO: update with AppState
    SemaphoreGuard lock(app_mutex_);
    EntityStateUpdate new_state_update;

    if (active_app != nullptr)
    {
        // Only send state updates to app using config with same identifier.
        if (strcmp(state.motor_state.config.id, active_app->app_id) == 0)
        {
            new_state_update = active_app->updateStateFromKnob(state.motor_state);
            active_app->updateStateFromSystem(state);
        }
    }

    return new_state_update;
}

void Apps::render()
{
    active_app->show(SCREEN_TRANSITION_NONE);
};

void Apps::setActive(int8_t id)
{
    SemaphoreGuard lock(app_mutex_);
    ScreenTransition transition = SCREEN_TRANSITION_NONE;
    if (active_id == MENU && id != MENU)
    {
        transition = SCREEN_TRANSITION_OPEN;
    }
    else if (active_id != MENU && id == MENU)
    {
        transition = SCREEN_TRANSITION_BACK;
    }

    if (id == MENU)
    {
        active_app = menu;
        active_id = MENU;
        active_app->show(transition);
        return;
    }
    LOGV(LOG_LEVEL_DEBUG, "Set active %d", id);
    active_id = id;
    if (apps[active_id] == nullptr)
    {
        // TODO: panic?
        LOGW("Null pointer instead of app");
    }
    else
    {
        active_app = apps[active_id];
        active_app->show(transition);
    }
}

App *Apps::loadApp(uint8_t position, std::string app_slug, char *app_id, char *friendly_name, char *entity_id)
{
    AppData app_data = {};
    sprintf(app_data.app_id, "%s", app_id);
    sprintf(app_data.friendly_name, "%s", friendly_name);
    sprintf(app_data.entity_id, "%s", entity_id);

    if (app_slug.compare(APP_SLUG_CLIMATE) == 0)
    {
        ClimateApp *app = new ClimateApp(screen_mutex_, app_id, friendly_name, entity_id);
        add(position, app);
        return app;
    }
    // else if (app_slug.compare(APP_SLUG_3D_PRINTER) == 0)
    // {
    //     PrinterChamberApp *app = new PrinterChamberApp(this->spr_, app_id);
    //     // app->friendly_name = friendly_name;
    //     sprintf(app->friendly_name, "%s", friendly_name);
    //     add(position, app);
    //     return app;
    // }
    else if (app_slug.compare(APP_SLUG_BLINDS) == 0)
    {
        BlindsApp *app = new BlindsApp(screen_mutex_, app_id, friendly_name, entity_id);
        add(position, app);
        return app;
    }
    else if (app_slug.compare(APP_SLUG_LIGHT_DIMMER) == 0)
    {
        LightDimmerApp *app = new LightDimmerApp(screen_mutex_, app_data);
        add(position, app);
        return app;
    }
    else if (app_slug.compare(APP_SLUG_SWITCH) == 0 || app_slug.compare(APP_SLUG_LIGHT_SWITCH) == 0)
    {
        bool is_light_switch = (app_slug.compare(APP_SLUG_LIGHT_SWITCH) == 0);
        SwitchApp *app = new SwitchApp(screen_mutex_, app_id, friendly_name, entity_id, is_light_switch);

        add(position, app);
        return app;
    }
    else if (app_slug.compare(APP_SLUG_SPOTIFY) == 0)
    {
        SpotifyApp *app = new SpotifyApp(screen_mutex_, app_id, friendly_name, entity_id);

        add(position, app);
        return app;
    }
    else if (app_slug.compare(APP_SLUG_SPEAKER) == 0)
    {
        SpeakerApp *app = new SpeakerApp(screen_mutex_, app_id, friendly_name, entity_id);
        add(position, app);
        return app;
    }
    else if (app_slug.compare(APP_SLUG_STOPWATCH) == 0)
    {
        StopwatchApp *app = new StopwatchApp(screen_mutex_, entity_id);
        // sprintf(app->friendly_name, "%s", friendly_name);
        add(position, app);
        return app;
    }
    else
    {
        LOGW("Can't find app with slug '%s'", app_slug.c_str());
    }
    return nullptr;
}
void Apps::updateMenu()
{
    {
        SemaphoreGuard lock(app_mutex_);
        menu = std::make_shared<MenuApp>(screen_mutex_);

        std::map<uint8_t, std::shared_ptr<App>>::iterator it;
        for (it = apps.begin(); it != apps.end(); it++)
        {
            menu->addItem((int8_t)it->first, it->second);
        }
    }

    active_id = MENU;
    setActive(MENU);
}

void Apps::setMotorNotifier(MotorNotifier *motor_notifier)
{
    this->motor_notifier = motor_notifier;

    std::map<uint8_t, std::shared_ptr<App>>::iterator it;
    for (it = apps.begin(); it != apps.end(); it++)
    {
        it->second->setMotorNotifier(motor_notifier);
    }
}
void Apps::triggerMotorConfigUpdate()
{
    if (this->motor_notifier != nullptr)
    {
        if (active_app != nullptr)
        {
            motor_notifier->requestUpdate(active_app->getMotorConfig());
        }
        else
        {
            motor_notifier->requestUpdate(blocked_motor_config);
            LOGW("No active app");
        }
    }
    else
    {
        LOGW("Motor_notifier is not set");
    }
}

void Apps::handleNavigationEvent(NavigationEvent event)
{
    int8_t next_app = DONT_NAVIGATE;

    // Ask once: an app may act on being asked (the home screen starts opening).
    int8_t target = DONT_NAVIGATE;
    switch (event)
    {
    case NavigationEvent::SHORT:
        target = active_app->navigationNext();
        break;
    case NavigationEvent::LONG:
        target = active_app->navigationBack();
        break;
    default:
        break;
    }

    switch (target)
    {
    case DONT_NAVIGATE:
        return;
    case DONT_NAVIGATE_UPDATE_MOTOR_CONFIG:
        break;
    default:
        next_app = target;
        break;
    }

    active_app->handleNavigation(event); // For settings app and future reimplementation of navigation

    if (next_app != DONT_NAVIGATE)
        setActive(next_app);

    motor_notifier->requestUpdate(active_app->getMotorConfig());
}

std::shared_ptr<App> Apps::find(uint8_t id)
{
    // TODO: add protection with array size
    return apps[id];
}
std::shared_ptr<App> Apps::find(char *app_id)
{
    std::map<uint8_t, std::shared_ptr<App>>::iterator it;
    for (it = apps.begin(); it != apps.end(); it++)
    {
        if (strcmp(it->second->app_id, app_id) == 0)
        {
            return it->second;
        }
    }
    return nullptr;
}

void Apps::setOSConfigNotifier(OSConfigNotifier *os_config_notifier)
{
    os_config_notifier_ = os_config_notifier;
}