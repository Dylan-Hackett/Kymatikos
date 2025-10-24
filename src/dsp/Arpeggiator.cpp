#include "Arpeggiator.h"
// #include <algorithm> // REMOVED - no longer needed (no std::vector)

// --- Namespace imports (local to this implementation file) ---
using namespace daisy;
using namespace daisysp;

Arpeggiator::Arpeggiator()
    : note_count_(0),
      rng_state_(0x12345678),
      scale_(nullptr),
      scale_size_(0),
      octave_jump_prob_(0.0f),
      note_callback_(nullptr),
      polyrhythm_ratio_(1.0f),
      next_trigger_time_(0.0f),
      current_time_(0.0f),
      sample_rate_(48000.0f),
      current_interval_(1.0f),
      step_index_(0),
      direction_(Forward)
{
    // Initialize fixed-size array to zero
    for (int i = 0; i < MAX_NOTES; ++i) {
        notes_[i] = 0;
    }
}

void Arpeggiator::Init(float samplerate) {
    sample_rate_ = samplerate;
    metro_.Init(1.0f, samplerate);
    next_trigger_time_ = 0.0f;
    current_time_ = 0.0f;
    UpdateInterval();
}

void Arpeggiator::SetScale(float* scale, int scale_size) {
    scale_ = scale;
    scale_size_ = scale_size;
}

void Arpeggiator::SetMainTempo(float tempo) {
    if (tempo < 0.1f) tempo = 0.1f;
    metro_.SetFreq(tempo);
    UpdateInterval();
}

void Arpeggiator::SetPolyrhythmRatio(float ratio) {
    if (ratio <= 0.0f) ratio = 1.0f;
    polyrhythm_ratio_ = ratio;
    UpdateInterval();
}

void Arpeggiator::SetOctaveJumpProbability(float probability) {
    octave_jump_prob_ = probability;
}

void Arpeggiator::SetNoteTriggerCallback(std::function<void(int)> cb) {
    note_callback_ = cb;
}

void Arpeggiator::SetDirection(Direction dir) {
    direction_ = dir;
    step_index_ = 0;
}

bool Arpeggiator::IsActive() const {
    return note_count_ > 0;
}

float Arpeggiator::GetMetroRate() {
    return metro_.GetFreq();
}

float Arpeggiator::GetCurrentInterval() const {
    return current_interval_;
}

void Arpeggiator::Process(size_t frames) {
    if (note_count_ == 0 || !note_callback_) return;
    for (size_t i = 0; i < frames; ++i) {
        current_time_ += 1.0f / sample_rate_;
        if (current_time_ >= next_trigger_time_) {
            TriggerNote();
            next_trigger_time_ += current_interval_;
        }
    }
}

void Arpeggiator::TriggerNote() {
    if (note_count_ == 0) return;
    int idx;
    if (direction_ == Random) {
        uint32_t rnd = Xorshift32();
        idx = rnd % note_count_;
    } else { // Forward or AsPlayed (use insertion order from notes_)
        idx = step_index_ % note_count_;
        ++step_index_;
    }
    if (note_callback_) {
        note_callback_(notes_[idx]);
    }
}

void Arpeggiator::UpdateInterval() {
    float main_interval = 1.0f / metro_.GetFreq();
    current_interval_ = main_interval / polyrhythm_ratio_;
}

void Arpeggiator::SetMainTempoFromKnob(float knob_value) {
    // Exponential tempo mapping for full knob response: 1 Hz to 30 Hz
    // knob_value is expected to be in 0-1 range
    float min_tempo = 1.0f;
    float max_tempo = 30.0f;
    float mapped_ratio = max_tempo / min_tempo;
    float tempo_hz = min_tempo * powf(mapped_ratio, knob_value);
    SetMainTempo(tempo_hz); // Call existing SetMainTempo
}

void Arpeggiator::SetPolyrhythmRatioFromKnob(float knob_value) {
    // Map knob_value (0-1 range from ADC) to polyrhythm ratio 0.5x to 2.0x
    float min_poly_ratio = 0.5f;
    float max_poly_ratio = 2.0f;
    float ratio = min_poly_ratio + knob_value * (max_poly_ratio - min_poly_ratio);
    SetPolyrhythmRatio(ratio); // Call existing SetPolyrhythmRatio
}

void Arpeggiator::UpdateHeldNotes(uint16_t current_touch_state, uint16_t last_touch_state) {
    uint16_t changed_pads = current_touch_state ^ last_touch_state;

    if (changed_pads != 0) {
        for (int i = 0; i < 12; ++i) { // Iterate through all 12 possible pads
            uint16_t mask = 1 << i;
            if (changed_pads & mask) { // If this pad's state changed
                if (current_touch_state & mask) { // Pad was pressed
                    // Add to list only if not already present
                    bool found = false;
                    for (int j = 0; j < note_count_; ++j) {
                        if (notes_[j] == i) {
                            found = true;
                            break;
                        }
                    }
                    // Add note if not found and we have space
                    if (!found && note_count_ < MAX_NOTES) {
                        notes_[note_count_] = i;  // ✅ No heap allocation
                        note_count_++;
                    }
                } else { // Pad was released
                    // Remove from list by shifting elements left
                    for (int j = 0; j < note_count_; ++j) {
                        if (notes_[j] == i) {
                            // Shift all elements after this one to the left
                            for (int k = j; k < note_count_ - 1; ++k) {
                                notes_[k] = notes_[k + 1];
                            }
                            note_count_--;
                            break;
                        }
                    }
                }
            }
        }
    }
}

uint32_t Arpeggiator::Xorshift32() {
    uint32_t x = rng_state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state_ = x;
    return x;
}

// Public helper to clear held notes list
void Arpeggiator::ClearNotes() {
    note_count_ = 0;
    step_index_ = 0; // reset step to avoid out-of-bounds on next use
} 