#include "ControlsManager.h"
#include <cstring>

ControlsManager::ControlsManager()
    : touch_state_(0),
      touch_cv_(0.0f),
      engine_index_(0),
      arp_enabled_(false),
      arp_clear_requested_(false),
      control_read_index_(0),
      control_write_index_(0),
      update_display_(false),
      smoothed_level_(0.0f),
      input_peak_(0.0f),
      was_arp_on_(false) {
    memset(adc_raw_values_, 0, sizeof(adc_raw_values_));
    for(int i = 0; i < 12; ++i) arp_led_ts_[i].store(0, std::memory_order_relaxed);
    control_buffers_[0] = ControlSnapshot{};
    control_buffers_[1] = ControlSnapshot{};
    audio_control_snapshot_ = ControlSnapshot{};
    latest_control_snapshot_ = ControlSnapshot{};
}

void ControlsManager::Init(float sample_rate) {
    arp_.Init(sample_rate);
    arp_.SetDirection(Arpeggiator::AsPlayed);
    arp_enabled_.store(false, std::memory_order_release);
    arp_clear_requested_.store(false, std::memory_order_release);
    control_read_index_ = 0;
    control_write_index_.store(0, std::memory_order_release);
    audio_control_snapshot_ = ControlSnapshot{};
    latest_control_snapshot_ = ControlSnapshot{};
}

void ControlsManager::SetArpEnabled(bool enabled) {
    arp_enabled_.store(enabled, std::memory_order_release);
    if(!enabled) {
        arp_clear_requested_.store(true, std::memory_order_release);
    }
}

bool ControlsManager::ConsumeArpClearRequest() {
    return arp_clear_requested_.exchange(false, std::memory_order_acq_rel);
}

void ControlsManager::UpdateControlSnapshot(const ControlSnapshot& snapshot) {
    uint8_t current_index = control_write_index_.load(std::memory_order_relaxed);
    uint8_t next_index = current_index ^ 1;
    control_buffers_[next_index] = snapshot;
    control_write_index_.store(next_index, std::memory_order_release);
    latest_control_snapshot_ = snapshot;
}

void ControlsManager::SyncAudioControlSnapshot() {
    uint8_t write_index = control_write_index_.load(std::memory_order_acquire);
    if(write_index != control_read_index_) {
        control_read_index_ = write_index;
        audio_control_snapshot_ = control_buffers_[control_read_index_];
    }
}
