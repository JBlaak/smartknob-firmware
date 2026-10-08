#if SK_DISPLAY
#include "display_task.h"
#include "semaphore_guard.h"
#include "util.h"
#include "esp_heap_caps.h"

#include "apps/switch/switch.h"
#include "apps/light_dimmer/light_dimmer.h"

#include "cJSON.h"

#define TFT_HOR_RES 240
#define TFT_VER_RES 240

#define LVGL_TASK_MAX_DELAY_MS (500)
#define LVGL_TASK_MIN_DELAY_MS (1)

DisplayTask::DisplayTask(const uint8_t task_core) : Task{"Display", 1024 * 5, 2, task_core}
{
    app_state_queue_ = xQueueCreate(1, sizeof(AppState));
    assert(app_state_queue_ != NULL);

    mutex_ = xSemaphoreCreateMutex();
    assert(mutex_ != NULL);
}

DisplayTask::~DisplayTask()
{

    vQueueDelete(app_state_queue_);
    vSemaphoreDelete(mutex_);
}

OnboardingFlow *DisplayTask::getOnboardingFlow()
{
    while (onboarding_flow == nullptr)
    {
        delay(50);
    }
    return onboarding_flow;
}

DemoApps *DisplayTask::getDemoApps()
{
    while (demo_apps == nullptr)
    {
        delay(50);
    }
    return demo_apps;
}

HassApps *DisplayTask::getHassApps()
{
    while (hass_apps == nullptr)
    {
        delay(50);
    }
    return hass_apps;
}

SpotifyStandalone *DisplayTask::getSpotifyStandalone()
{
    while (spotify_standalone == nullptr)
    {
        delay(50);
    }
    return spotify_standalone;
}

ErrorHandlingFlow *DisplayTask::getErrorHandlingFlow()
{
    while (error_handling_flow == nullptr)
    {
        delay(50);
    }
    return error_handling_flow;
}

void DisplayTask::run()
{
    ledcSetup(LEDC_CHANNEL_LCD_BACKLIGHT, 5000, SK_BACKLIGHT_BIT_DEPTH);
    ledcAttachPin(PIN_LCD_BACKLIGHT, LEDC_CHANNEL_LCD_BACKLIGHT);
    ledcWrite(LEDC_CHANNEL_LCD_BACKLIGHT, (1 << SK_BACKLIGHT_BIT_DEPTH) - 1);

    lv_init();
    lv_skdk_create();
    lv_disp_drv_t *disp_drv = lv_skdk_get_disp_drv();

    onboarding_flow = new OnboardingFlow(mutex_);
    demo_apps = new DemoApps(mutex_);
    hass_apps = new HassApps(mutex_);
    spotify_standalone = new SpotifyStandalone(mutex_);
    error_handling_flow = new ErrorHandlingFlow(mutex_);
    while (display_os_mode == UNSET)
    {
        delay(50);
    }

    while (1)
    {
        {
            SemaphoreGuard lock(mutex_);
            lv_task_handler();
        }
#if SK_UI_DEBUG
        if (snapshot_requested_)
        {
            snapshot_requested_ = false;
            dumpSnapshot();
        }
        logPerf();
#endif
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void DisplayTask::requestSnapshot()
{
    snapshot_requested_ = true;
}

void DisplayTask::dumpSnapshot()
{
    lv_img_dsc_t *snapshot;
    {
        SemaphoreGuard lock(mutex_);
        snapshot = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    }
    if (snapshot == NULL)
    {
        LOGE("SNAP failed: out of memory");
        return;
    }

    const uint16_t width = snapshot->header.w;
    const uint16_t height = snapshot->header.h;
    const uint16_t half = width / 2;
    static const char hex[] = "0123456789abcdef";
    char line[half * 4 + 1];

    LOGI("SNAP begin %d %d", width, height);
    const uint16_t *pixels = (const uint16_t *)snapshot->data;
    for (uint16_t y = 0; y < height; y++)
    {
        for (uint8_t part = 0; part < 2; part++)
        {
            const uint16_t *row = pixels + y * width + part * half;
            for (uint16_t x = 0; x < half; x++)
            {
                line[x * 4 + 0] = hex[(row[x] >> 12) & 0xF];
                line[x * 4 + 1] = hex[(row[x] >> 8) & 0xF];
                line[x * 4 + 2] = hex[(row[x] >> 4) & 0xF];
                line[x * 4 + 3] = hex[row[x] & 0xF];
            }
            line[half * 4] = '\0';
            LOGI("SNAP %d %d %s", y, part, line);
            vTaskDelay(pdMS_TO_TICKS(4));
        }
    }
    LOGI("SNAP end");

    SemaphoreGuard lock(mutex_);
    lv_snapshot_free(snapshot);
}

// Logs frame rate and frame cost every two seconds while the screen is changing.
void DisplayTask::logPerf()
{
    if (millis() - perf_logged_at_ < 2000)
    {
        return;
    }
    uint32_t elapsed = millis() - perf_logged_at_;
    perf_logged_at_ = millis();

    uint32_t frames, total_ms, max_ms, pixels, flush_ms;
    lv_skdk_take_perf(&frames, &total_ms, &max_ms, &pixels, &flush_ms);
    if (frames == 0)
    {
        return;
    }
    LOGI("PERF %u frames in %u ms: %u ms avg (%u ms sending), %u ms max, %u px/frame", frames, elapsed, total_ms / frames, flush_ms / frames, max_ms, pixels / frames);
}

QueueHandle_t DisplayTask::getKnobStateQueue()
{
    return app_state_queue_;
}

void DisplayTask::setBrightness(uint16_t brightness)
{
    SemaphoreGuard lock(mutex_);
    lv_skdk_get_lcd()->setBrightness((((float)brightness / UINT16_MAX) * 255)); // Quickly implemented brightness for lvgl with old (current) impl.
}

void DisplayTask::enableOnboarding()
{
    display_os_mode = ONBOARDING;
    onboarding_flow->render();
    onboarding_flow->triggerMotorConfigUpdate();
}

void DisplayTask::enableDemo()
{
    display_os_mode = DEMO;
    demo_apps->render();
    demo_apps->triggerMotorConfigUpdate();
}

void DisplayTask::enableHass()
{
    display_os_mode = HASS;
    hass_apps->render();
    hass_apps->triggerMotorConfigUpdate();
}

void DisplayTask::enableSpotify()
{

    display_os_mode = SPOTIFY;
    spotify_standalone->render();
    spotify_standalone->triggerMotorConfigUpdate();
}
#endif