#include "app_menu.h"

#include <time.h>

// Seconds without a turn or press before the ring folds into the clock.
#ifndef SK_HOME_IDLE_MS
#define SK_HOME_IDLE_MS 8000
#endif

static const float RING_RADIUS = 86;
static const lv_coord_t CENTER = 120;
static const lv_coord_t FOCUSED_SIZE = 50;
static const lv_coord_t UNFOCUSED_SIZE = 34;
static const lv_coord_t DOT_SIZE = 10;
static const lv_coord_t TEXT_WIDTH = 180;
static const lv_coord_t TEXT_TOP = 97;
static const lv_coord_t TEXT_ROLL = 16; // how far a label slides per step
static const lv_coord_t GROWN_SIZE = 340;

static int32_t lerp(int32_t from, int32_t to, int32_t amount)
{
    return from + (to - from) * amount / 1000;
}

MenuApp::MenuApp(SemaphoreHandle_t mutex) : App(mutex)
{
    sprintf(this->app_id, "%s", "MENU");
    back_ = MENU;
    motor_config = PB_SmartKnobConfig{
        0,
        0,
        0,
        0,
        -1, // max position < min position indicates no bounds
        25 * PI / 180,
        2,
        1,
        0.55,
        "MENU",
        0,
        {},
        0,
        20,
    };

    SemaphoreGuard lock(mutex_);
    lv_obj_set_style_bg_color(screen, SK_COLOR_BACKGROUND, 0);

    rim_dot_ = sk_circle_create(screen, 6, SK_COLOR_TEXT);
    lv_obj_set_pos(rim_dot_, CENTER - 3, 3);

    clock_ = lv_obj_create(screen);
    lv_obj_remove_style_all(clock_);
    lv_obj_set_size(clock_, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(clock_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    time_label_ = sk_label_create(clock_, &figtree_600_54, SK_COLOR_TEXT);
    lv_obj_set_style_text_letter_space(time_label_, -1, 0);
    lv_obj_align(time_label_, LV_ALIGN_CENTER, 0, -10);
    date_label_ = sk_label_create(clock_, &figtree_600_12, SK_COLOR_TEXT_SECONDARY);
    lv_obj_set_style_text_letter_space(date_label_, 1, 0);
    lv_obj_align(date_label_, LV_ALIGN_CENTER, 0, 26);

    // Covers the screen in the app's tint while an app opens. Created before
    // the items are added, it would sit below them, so it is moved to the
    // front when it is used.
    grow_ = sk_circle_create(screen, FOCUSED_SIZE, SK_COLOR_TEXT);
    lv_obj_add_flag(grow_, LV_OBJ_FLAG_HIDDEN);

    idle_timer_ = lv_timer_create(idleTimerCb, 250, this);
    last_input_ms_ = millis();
    updateClock();
    setIdleAmount(0);
}

MenuApp::~MenuApp()
{
    SemaphoreGuard lock(mutex_);
    lv_anim_del(this, NULL);
    if (idle_timer_ != nullptr)
    {
        lv_timer_del(idle_timer_);
    }
}

void MenuApp::addItem(int8_t id, std::shared_ptr<App> app)
{
    SemaphoreGuard lock(mutex_);
    Item item;
    item.id = id;
    item.app = app;

    item.text = lv_obj_create(screen);
    lv_obj_remove_style_all(item.text);
    lv_obj_set_size(item.text, TEXT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(item.text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(item.text, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(item.text, 5, 0);
    lv_obj_clear_flag(item.text, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    item.name = sk_label_create(item.text, &figtree_600_23, SK_COLOR_TEXT);
    lv_obj_set_width(item.name, TEXT_WIDTH);
    lv_obj_set_style_text_align(item.name, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(item.name, LV_LABEL_LONG_DOT);
    lv_label_set_text(item.name, app->friendly_name);

    item.status = sk_label_create(item.text, &figtree_500_13, SK_COLOR_TEXT_SECONDARY);
    lv_obj_set_width(item.status, TEXT_WIDTH);
    lv_obj_set_style_text_align(item.status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(item.status, LV_LABEL_LONG_DOT);

    item.bubble = sk_circle_create(screen, UNFOCUSED_SIZE, SK_COLOR_SURFACE);
    item.glyph = sk_glyph_create(item.bubble, app->glyph, app->tint);
    lv_obj_align(item.glyph, LV_ALIGN_CENTER, 0, 0);

    items_.push_back(item);
    lv_obj_move_foreground(grow_);
    layout();
}

int MenuApp::focusedIndex()
{
    int n = items_.size();
    if (n == 0)
    {
        return 0;
    }
    int32_t index = position_ - position_offset_;
    return ((index % n) + n) % n;
}

EntityStateUpdate MenuApp::updateStateFromKnob(PB_SmartKnobState state)
{
    // Restores the ring to where it was when coming back from an app.
    motor_config.position = state.current_position;
    motor_config.position_nonce = state.current_position;

    int32_t delta = state.current_position - position_;
    if (delta == 0 || items_.empty())
    {
        return EntityStateUpdate{};
    }
    position_ = state.current_position;
    last_input_ms_ = millis();

    {
        SemaphoreGuard lock(mutex_);
        if (idle_)
        {
            // The first turn only wakes the screen.
            position_offset_ += delta;
            setIdle(false);
        }
        else
        {
            animateRingTo((position_ - position_offset_) * 1000);
        }
    }
    next_ = items_[focusedIndex()].id;
    return EntityStateUpdate{};
}

int8_t MenuApp::navigationNext()
{
    last_input_ms_ = millis();
    if (items_.empty())
    {
        return DONT_NAVIGATE;
    }

    SemaphoreGuard lock(mutex_);
    if (idle_)
    {
        setIdle(false);
        return DONT_NAVIGATE;
    }

    // Settle the ring on the app being opened, then grow it.
    lv_anim_del(this, ringAnimCb);
    setRingPos((position_ - position_offset_) * 1000);

    Item &item = items_[focusedIndex()];
    lv_obj_set_style_bg_color(grow_, item.app->tint, 0);
    lv_obj_move_foreground(grow_);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, this);
    lv_anim_set_exec_cb(&a, openAnimCb);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_time(&a, SK_ANIM_OPEN_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);

    next_ = item.id;
    return item.id;
}

void MenuApp::show(ScreenTransition transition)
{
    SemaphoreGuard lock(mutex_);
    lv_anim_del(this, NULL);
    last_input_ms_ = millis();
    idle_ = false;
    idle_amount_ = 0;
    open_amount_ = 0;
    lv_obj_add_flag(grow_, LV_OBJ_FLAG_HIDDEN);
    ring_pos_ = (position_ - position_offset_) * 1000;
    for (Item &item : items_)
    {
        lv_label_set_text(item.status, item.app->statusText().c_str());
    }
    updateClock();
    layout();

    if (transition == SCREEN_TRANSITION_BACK)
    {
        lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, SK_ANIM_FADE_MS, 0, false);
    }
    else
    {
        lv_scr_load(screen);
    }
}

void MenuApp::refreshStatus()
{
    SemaphoreGuard lock(mutex_);
    for (Item &item : items_)
    {
        std::string status = item.app->statusText();
        if (strcmp(lv_label_get_text(item.status), status.c_str()) != 0)
        {
            lv_label_set_text(item.status, status.c_str());
        }
    }
}

// Callers hold mutex_.
void MenuApp::animateRingTo(int32_t ring_pos)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, this);
    lv_anim_set_exec_cb(&a, ringAnimCb);
    lv_anim_set_values(&a, ring_pos_, ring_pos);
    lv_anim_set_time(&a, SK_ANIM_SPRING_MS);
    lv_anim_set_path_cb(&a, sk_anim_path_spring);
    lv_anim_start(&a);
}

// Callers hold mutex_.
void MenuApp::setIdle(bool idle)
{
    idle_ = idle;
    if (!idle)
    {
        last_input_ms_ = millis();
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, this);
    lv_anim_set_exec_cb(&a, idleAnimCb);
    lv_anim_set_values(&a, idle_amount_, idle ? 1000 : 0);
    lv_anim_set_time(&a, idle ? 700 : 420);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

void MenuApp::setRingPos(int32_t ring_pos)
{
    ring_pos_ = ring_pos;
    layout();
}

void MenuApp::setIdleAmount(int32_t idle_amount)
{
    idle_amount_ = idle_amount;
    layout();
}

void MenuApp::setOpenAmount(int32_t open_amount)
{
    open_amount_ = open_amount;
    if (open_amount == 0)
    {
        lv_obj_add_flag(grow_, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_coord_t size = lerp(FOCUSED_SIZE, GROWN_SIZE, open_amount);
        lv_coord_t center_y = lerp(CENTER - RING_RADIUS, CENTER, open_amount);
        lv_obj_set_size(grow_, size, size);
        lv_obj_set_pos(grow_, CENTER - size / 2, center_y - size / 2);
        lv_obj_clear_flag(grow_, LV_OBJ_FLAG_HIDDEN);
    }
    layout();
}

// Places everything for the current ring position, idle and open amounts.
// Callers hold mutex_.
void MenuApp::layout()
{
    int32_t n = items_.size();
    lv_opa_t awake_opa = 255 * (1000 - idle_amount_) / 1000 * (1000 - open_amount_) / 1000;

    // Opacity goes on each part (bg_opa, img_opa, text_opa) rather than the
    // object's opa, which LVGL draws through an extra layer.
    lv_obj_set_style_text_opa(clock_, 255 * idle_amount_ / 1000 * (1000 - open_amount_) / 1000, 0);
    if (idle_amount_ == 0)
    {
        lv_obj_add_flag(clock_, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(clock_, LV_OBJ_FLAG_HIDDEN);
    }

    if (n == 0)
    {
        lv_obj_add_flag(rim_dot_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    int32_t period = n * 1000;
    int32_t nearest = 0;
    int32_t nearest_distance = INT32_MAX;
    for (int32_t i = 0; i < n; i++)
    {
        Item &item = items_[i];

        // How far this item is from the top, in items x 1000, the short way round.
        int32_t d = ((i * 1000 - ring_pos_) % period + period) % period;
        if (d > period / 2)
        {
            d -= period;
        }
        if (abs(d) < nearest_distance)
        {
            nearest_distance = abs(d);
            nearest = i;
        }
        int32_t focus = LV_MAX(0, 1000 - abs(d));

        float angle = 2 * PI * d / period;
        lv_coord_t x = CENTER + RING_RADIUS * sinf(angle);
        lv_coord_t y = CENTER - RING_RADIUS * cosf(angle);
        lv_coord_t size = lerp(lerp(UNFOCUSED_SIZE, FOCUSED_SIZE, focus), DOT_SIZE, idle_amount_);
        lv_obj_set_size(item.bubble, size, size);
        lv_obj_set_pos(item.bubble, x - size / 2, y - size / 2);

        lv_color_t awake_bg = lv_color_mix(item.app->tint, SK_COLOR_SURFACE, 255 * focus / 1000);
        lv_obj_set_style_bg_color(item.bubble, lv_color_mix(item.app->tint, awake_bg, 255 * idle_amount_ / 1000), 0);
        lv_obj_set_style_img_recolor(item.glyph, lv_color_mix(SK_COLOR_BACKGROUND, item.app->tint, 255 * focus / 1000), 0);

        int32_t opa = lerp(lerp(150, 255, focus), lerp(100, 230, focus), idle_amount_);
        if (open_amount_ > 0 && focus < 1000)
        {
            opa = opa * (1000 - open_amount_) / 1000;
        }
        lv_obj_set_style_bg_opa(item.bubble, opa, 0);
        lv_obj_set_style_img_opa(item.glyph, opa * (1000 - idle_amount_) / 1000, 0);

        // The name and status roll with the ring.
        int32_t text_opa = LV_MAX(0, 1000 - abs(d) * 2) * awake_opa / 1000;
        if (text_opa == 0)
        {
            lv_obj_add_flag(item.text, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(item.text, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_opa(item.text, text_opa, 0);
            lv_obj_set_pos(item.text, CENTER - TEXT_WIDTH / 2, TEXT_TOP + TEXT_ROLL * d / 1000);
        }
    }

    lv_obj_clear_flag(rim_dot_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(rim_dot_, items_[nearest].app->tint, 0);
    lv_obj_set_style_bg_opa(rim_dot_, awake_opa, 0);
}

// Callers hold mutex_.
void MenuApp::updateClock()
{
    time_t now;
    time(&now);
    struct tm local;
    localtime_r(&now, &local);

    char time_text[8];
    char date_text[24] = "";
    if (local.tm_year < (2024 - 1900))
    {
        // Not synced with a time server yet.
        snprintf(time_text, sizeof(time_text), "--:--");
    }
    else
    {
        char weekday[8];
        char month[8];
        strftime(time_text, sizeof(time_text), "%H:%M", &local);
        strftime(weekday, sizeof(weekday), "%a", &local);
        strftime(month, sizeof(month), "%b", &local);
        snprintf(date_text, sizeof(date_text), "%s %d %s", weekday, local.tm_mday, month);
        for (char *c = date_text; *c; c++)
        {
            *c = toupper(*c);
        }
    }

    if (strcmp(time_text, shown_time_) != 0)
    {
        strcpy(shown_time_, time_text);
        lv_label_set_text(time_label_, time_text);
        lv_label_set_text(date_label_, date_text);
    }
}

void MenuApp::ringAnimCb(void *var, int32_t value)
{
    ((MenuApp *)var)->setRingPos(value);
}

void MenuApp::idleAnimCb(void *var, int32_t value)
{
    ((MenuApp *)var)->setIdleAmount(value);
}

void MenuApp::openAnimCb(void *var, int32_t value)
{
    ((MenuApp *)var)->setOpenAmount(value);
}

// Runs in the display task, inside lv_task_handler, so the lock is held.
void MenuApp::idleTimerCb(lv_timer_t *timer)
{
    MenuApp *self = (MenuApp *)timer->user_data;
    self->updateClock();
    if (!self->idle_ && self->open_amount_ == 0 && lv_scr_act() == self->screen && millis() - self->last_input_ms_ > SK_HOME_IDLE_MS)
    {
        self->setIdle(true);
    }
}
