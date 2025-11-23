#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include "daisy.h"
#include "daisysp.h"
#include <functional>
// #include <vector>  // REMOVED - replaced with fixed-size array for RT safety

// NOTE: using namespace directives removed from header to avoid namespace pollution
// Implementation file (.cpp) should add using namespace as needed locally

class Arpeggiator {
public:
    Arpeggiator();
    void Init(float samplerate);
    void SetScale(float* scale, int scale_size);
    void SetMainTempo(float tempo);             // Main tempo in Hz
    void SetPolyrhythmRatio(float ratio);       // Ratio for polyrhythm
    void SetOctaveJumpProbability(float probability); // 0.0f to 1.0f
    void Process(size_t frames);                // Call each block for scheduling

    void SetNoteTriggerCallback(std::function<void(int)> cb);

    bool IsActive() const;
    float GetMetroRate();
    float GetCurrentInterval() const;

    enum Direction { Forward, Random, AsPlayed };
    void SetDirection(Direction dir);

    // New method to update notes based on touch state
    void UpdateHeldNotes(uint16_t current_touch_state, uint16_t last_touch_state);

    // New methods for setting tempo and polyrhythm from knob values
    void SetMainTempoFromKnob(float knob_value); // knob_value is 0-1 range
    void SetPolyrhythmRatioFromKnob(float knob_value); // knob_value is 0-1 range

    // Clears all currently held notes (useful when ARP is disabled)
    void ClearNotes();

private:
    // Fixed-size array for real-time safety (no heap allocations)
    static constexpr int MAX_NOTES = 12;  // Maximum 12 touch pads
    int notes_[MAX_NOTES];                 // Fixed-size note buffer
    int note_count_;                       // Number of active notes

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
