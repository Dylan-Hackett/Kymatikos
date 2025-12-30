#ifndef CONTROLS_H
#define CONTROLS_H

#include <atomic>
#include <cstdint>
#include "Arpeggiator.h"

class ControlsManager {
public:
    struct ControlSnapshot {
        float clouds_position = 0.0f;
        float clouds_size = 0.5f;
        float clouds_density = 0.0f;
        float clouds_texture = 0.0f;
        float clouds_feedback = 0.0f;
        float clouds_reverb = 0.0f;
        float clouds_dry_wet = 1.0f;
        float clouds_pitch = 0.0f;
        float master_volume = 1.0f;
    };

    ControlsManager();
    ~ControlsManager() = default;

    void Init(float sample_rate);

    uint16_t GetCurrentTouchState() const { return touch_state_.load(std::memory_order_acquire); }
    void SetCurrentTouchState(uint16_t s) { touch_state_.store(s, std::memory_order_release); }
    float GetTouchCVValue() const { return touch_cv_.load(std::memory_order_relaxed); }
    void SetTouchCVValue(float v) { touch_cv_.store(v, std::memory_order_relaxed); }

    Arpeggiator& GetArpeggiator() { return arp_; }
    bool IsArpEnabled() const { return arp_enabled_.load(std::memory_order_acquire); }
    void SetArpEnabled(bool enabled);
    bool ConsumeArpClearRequest();

    float* GetADCRawValues() { return adc_raw_values_; }

    float GetInputPeakLevel() const { return input_peak_.load(std::memory_order_relaxed); }
    void SetInputPeakLevel(float v) { input_peak_.store(v, std::memory_order_relaxed); }

    void UpdateControlSnapshot(const ControlSnapshot& snapshot);
    void SyncAudioControlSnapshot();
    const ControlSnapshot& GetAudioControlSnapshot() const { return audio_snap_; }
    const ControlSnapshot& GetLatestControlSnapshot() const { return latest_snap_; }

    bool ShouldUpdateDisplay() const { return update_display_.load(std::memory_order_relaxed); }
    void SetUpdateDisplay(bool v) { update_display_.store(v, std::memory_order_relaxed); }
    float GetSmoothedOutputLevel() const { return smoothed_level_.load(std::memory_order_relaxed); }
    void SetSmoothedOutputLevel(float v) { smoothed_level_.store(v, std::memory_order_relaxed); }

    uint32_t GetArpLEDTimestamp(int i) const { return arp_led_ts_[i].load(std::memory_order_relaxed); }
    void SetArpLEDTimestamp(int i, uint32_t t) { arp_led_ts_[i].store(t, std::memory_order_relaxed); }

    bool WasArpOn() const { return was_arp_on_; }
    void SetWasArpOn(bool v) { was_arp_on_ = v; }

    static const uint32_t ARP_LED_DURATION_MS = 100;

private:
    std::atomic<uint16_t> touch_state_;
    std::atomic<float> touch_cv_;

    Arpeggiator arp_;
    std::atomic<bool> arp_enabled_;
    std::atomic<bool> arp_clear_requested_;

    float adc_raw_values_[12];

    ControlSnapshot buffers_[2];
    ControlSnapshot audio_snap_;
    ControlSnapshot latest_snap_;
    uint8_t read_idx_;
    std::atomic<uint8_t> write_idx_;

    std::atomic<bool> update_display_;
    std::atomic<float> smoothed_level_;
    std::atomic<float> input_peak_;
    std::atomic<uint32_t> arp_led_ts_[12];

    bool was_arp_on_;
};

#endif
