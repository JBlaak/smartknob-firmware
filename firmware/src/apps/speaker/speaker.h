#pragma once
#include "../app.h"

// A speaker: turning sets the volume on the rim, pressing plays or pauses, and
// the middle shows what is playing, with little bars moving while it plays.
//
// State to the controller:   {"volume": 0-100, "playing": bool}
// State from the controller: {"volume": 0-100, "playing": bool, "track": "...", "artist": "..."}
// (all fields optional). The whole state has to fit in MQTTStateUpdate.state, so
// the controller keeps track and artist short.
class SpeakerApp : public App
{
public:
    SpeakerApp(SemaphoreHandle_t mutex, char *app_id, char *friendly_name, char *entity_id);
    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state);
    void updateStateFromHASS(MQTTStateUpdate mqtt_state_update);

    int8_t navigationNext() override;
    std::string statusText() override;

private:
    void initScreen();
    void render();
    void setPlayingAnimation(bool playing);
    EntityStateUpdate stateUpdate();

    SkRimArc *arc_;
    lv_obj_t *track_label_;
    lv_obj_t *artist_label_;
    lv_obj_t *bars_;
    lv_obj_t *bar_[3];
    lv_obj_t *pause_;
    lv_obj_t *volume_label_;

    uint8_t volume_ = 0;
    uint8_t last_volume_ = 0;
    bool playing_ = false;
    bool playing_toggled_ = false;
    bool animating_ = false;
    bool has_track_ = false;
};
