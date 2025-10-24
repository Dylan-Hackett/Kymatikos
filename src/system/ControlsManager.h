#ifndef CONTROLS_MANAGER_H
#define CONTROLS_MANAGER_H

#include <atomic>
#include <cstdint>
#include "Arpeggiator.h"

/**
 * ControlsManager encapsulates all control state and processing:
 * - ADC raw values
 * - Processed knob values
 * - Touch sensor state
 * - Engine selection state
 * - Arpeggiator state
 * - LED blink timestamps
 *
 * This eliminates 15+ global variables and provides centralized
 * control processing logic.
 */
class ControlsManager {
public:
    struct ControlSnapshot {
        float pitch = 0.0f;
        float harm_knob = 0.0f;
        float timbre_knob = 0.0f;
        float morph_knob = 0.0f;
        float env_attack = 0.0f;
        float env_release = 0.0f;
        float delay_time = 0.0f;
        float delay_mix_feedback = 0.0f;
        float mod_wheel = 0.0f;
    };

    ControlsManager();
    ~ControlsManager() = default;

    // Initialize with sample rate
    void Init(float sample_rate);

    // Touch sensor state (returns references for backward compatibility)
    volatile uint16_t& GetCurrentTouchState() { return current_touch_state_; }
    volatile float& GetTouchCVValue() { return touch_cv_value_; }

    // Engine selection (returns references for backward compatibility)
    volatile int& GetCurrentEngineIndex() { return current_engine_index_; }
    volatile bool& IsEngineChanged() { return engine_changed_flag_; }

    // Arpeggiator
    Arpeggiator& GetArpeggiator() { return arp_; }
    bool IsArpEnabled() const { return arp_enabled_.load(std::memory_order_acquire); }
    void SetArpEnabled(bool enabled);
    bool ConsumeArpClearRequest();

    // ADC raw values
    volatile float* GetADCRawValues() { return adc_raw_values_; }
    float GetADCRawValue(int index) const { return adc_raw_values_[index]; }
    void SetADCRawValue(int index, float value) { adc_raw_values_[index] = value; }

    // Control snapshot access
    void UpdateControlSnapshot(const ControlSnapshot& snapshot);
    void SyncAudioControlSnapshot();
    const ControlSnapshot& GetAudioControlSnapshot() const { return audio_control_snapshot_; }
    const ControlSnapshot& GetLatestControlSnapshot() const { return latest_control_snapshot_; }

    // Display state (returns references for backward compatibility)
    volatile bool& ShouldUpdateDisplay() { return update_display_; }
    volatile float& GetSmoothedOutputLevel() { return smoothed_output_level_; }

    // ARP LED blink timestamps
    volatile uint32_t* GetArpLEDTimestamps() { return arp_led_timestamps_; }
    uint32_t GetArpLEDTimestamp(int index) const { return arp_led_timestamps_[index]; }
    void SetArpLEDTimestamp(int index, uint32_t timestamp) { arp_led_timestamps_[index] = timestamp; }

    // Engine retrigger phase (0=inactive, 2=trigger low, 1=trigger high)
    volatile int& GetEngineRetriggerPhase() { return engine_retrigger_phase_; }

    // Arpeggiator state change tracking
    bool& WasArpOn() { return was_arp_on_; }

    static const uint32_t ARP_LED_DURATION_MS = 100;

private:
    // Touch sensor state
    volatile uint16_t current_touch_state_;
    volatile float touch_cv_value_;

    // Engine selection
    volatile int current_engine_index_;
    volatile bool engine_changed_flag_;

    // Arpeggiator
    Arpeggiator arp_;
    std::atomic<bool> arp_enabled_;
    std::atomic<bool> arp_clear_requested_;

    // ADC raw values (12 channels)
    volatile float adc_raw_values_[12];

    // Control snapshots for UI / audio handoff
    ControlSnapshot control_buffers_[2];
    ControlSnapshot audio_control_snapshot_;
    ControlSnapshot latest_control_snapshot_;
    uint8_t control_read_index_;
    std::atomic<uint8_t> control_write_index_;

    // Display state
    volatile bool update_display_;
    volatile float smoothed_output_level_;

    // ARP LED blink timestamps
    volatile uint32_t arp_led_timestamps_[12];

    // Engine retrigger phase (0=inactive, 2=trigger low, 1=trigger high)
    volatile int engine_retrigger_phase_;

    // Arpeggiator state change tracking
    bool was_arp_on_;
};

#endif // CONTROLS_MANAGER_H
