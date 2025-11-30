#ifndef CONTROLS_MANAGER_H
#define CONTROLS_MANAGER_H

#include <atomic>
#include <cstdint>
#include "Arpeggiator.h"

class ControlsManager {
public:
    struct ControlSnapshot {
        float pitch = 0.0f;
        float position_knob = 0.0f;
        float density_knob = 0.0f;
        float blend_knob = 0.0f;
        float clouds_position = 0.0f;
        float clouds_size = 0.5f;
        float clouds_density = 0.0f;
        float clouds_texture = 0.0f;
        float clouds_feedback = 0.0f;
        float clouds_reverb = 0.0f;
        float clouds_dry_wet = 1.0f;
        float clouds_pitch = 0.0f;
        float master_volume = 1.0f;
        float mod_wheel = 0.0f;
    };

    ControlsManager();
    ~ControlsManager() = default;

    void Init(float sample_rate);

    // Touch state (atomic for ISR/main safety)
    uint16_t GetCurrentTouchState() const { return touch_state_.load(std::memory_order_acquire); }
    void SetCurrentTouchState(uint16_t s) { touch_state_.store(s, std::memory_order_release); }
    float GetTouchCVValue() const { return touch_cv_.load(std::memory_order_relaxed); }
    void SetTouchCVValue(float v) { touch_cv_.store(v, std::memory_order_relaxed); }

    // Engine selection
    int GetCurrentEngineIndex() const { return engine_index_.load(std::memory_order_relaxed); }
    void SetCurrentEngineIndex(int i) { engine_index_.store(i, std::memory_order_relaxed); }

    // Arpeggiator
    Arpeggiator& GetArpeggiator() { return arp_; }
    bool IsArpEnabled() const { return arp_enabled_.load(std::memory_order_acquire); }
    void SetArpEnabled(bool enabled);
    bool ConsumeArpClearRequest();

    // ADC raw values (main thread only)
    float* GetADCRawValues() { return adc_raw_values_; }
    float GetADCRawValue(int index) const { return adc_raw_values_[index]; }

    // Peak level (ISR writes, main reads)
    float GetInputPeakLevel() const { return input_peak_.load(std::memory_order_relaxed); }
    void SetInputPeakLevel(float v) { input_peak_.store(v, std::memory_order_relaxed); }

    // Control snapshot access
    void UpdateControlSnapshot(const ControlSnapshot& snapshot);
    void SyncAudioControlSnapshot();
    const ControlSnapshot& GetAudioControlSnapshot() const { return audio_control_snapshot_; }
    const ControlSnapshot& GetLatestControlSnapshot() const { return latest_control_snapshot_; }

    // Display state (ISR writes, main reads)
    bool ShouldUpdateDisplay() const { return update_display_.load(std::memory_order_relaxed); }
    void SetUpdateDisplay(bool v) { update_display_.store(v, std::memory_order_relaxed); }
    float GetSmoothedOutputLevel() const { return smoothed_level_.load(std::memory_order_relaxed); }
    void SetSmoothedOutputLevel(float v) { smoothed_level_.store(v, std::memory_order_relaxed); }

    // ARP LED timestamps (ISR writes via callback, main reads)
    uint32_t GetArpLEDTimestamp(int i) const { return arp_led_ts_[i].load(std::memory_order_relaxed); }
    void SetArpLEDTimestamp(int i, uint32_t t) { arp_led_ts_[i].store(t, std::memory_order_relaxed); }

    bool WasArpOn() const { return was_arp_on_; }
    void SetWasArpOn(bool v) { was_arp_on_ = v; }

    static const uint32_t ARP_LED_DURATION_MS = 100;

private:
    std::atomic<uint16_t> touch_state_;
    std::atomic<float> touch_cv_;
    std::atomic<int> engine_index_;

    Arpeggiator arp_;
    std::atomic<bool> arp_enabled_;
    std::atomic<bool> arp_clear_requested_;

    float adc_raw_values_[12];

    ControlSnapshot control_buffers_[2];
    ControlSnapshot audio_control_snapshot_;
    ControlSnapshot latest_control_snapshot_;
    uint8_t control_read_index_;
    std::atomic<uint8_t> control_write_index_;

    std::atomic<bool> update_display_;
    std::atomic<float> smoothed_level_;
    std::atomic<float> input_peak_;
    std::atomic<uint32_t> arp_led_ts_[12];

    bool was_arp_on_;
};

#endif // CONTROLS_MANAGER_H
