#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include "daisy.h"
#include "daisysp.h"
#include <functional>

class Arpeggiator {
public:
    Arpeggiator();
    void Init(float samplerate);
    void SetScale(float* scale, int scale_size);
    void SetMainTempo(float tempo);
    void SetPolyrhythmRatio(float ratio);
    void SetOctaveJumpProbability(float probability);
    void Process(size_t frames);

    void SetNoteTriggerCallback(std::function<void(int)> cb);

    bool IsActive() const;
    float GetMetroRate();
    float GetCurrentInterval() const;

    enum Direction { Forward, Random, AsPlayed };
    void SetDirection(Direction dir);

    void UpdateHeldNotes(uint16_t current_touch_state, uint16_t last_touch_state);
    void SetMainTempoFromKnob(float knob_value);
    void SetPolyrhythmRatioFromKnob(float knob_value);
    void ClearNotes();

private:
    static constexpr int MAX_NOTES = 12;
    int notes_[MAX_NOTES];
    int note_count_;

    daisysp::Metro metro_;
    uint32_t rng_state_;
    float* scale_;
    int scale_size_;
    float octave_jump_prob_;
    std::function<void(int)> note_callback_;

    uint32_t Xorshift32();
    void TriggerNote();

    float polyrhythm_ratio_;
    float next_trigger_time_;
    float current_time_;
    float sample_rate_;
    float current_interval_;
    int step_index_;
    Direction direction_;

    void UpdateInterval();
};

#endif // ARPEGGIATOR_H

