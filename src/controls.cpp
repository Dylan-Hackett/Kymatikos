#include "controls.h"
#include <cstring>

ControlsManager::ControlsManager()
    : touch_state_(0),
      touch_cv_(0.0f),
      arp_enabled_(false),
      arp_clear_requested_(false),
      read_idx_(0),
      write_idx_(0),
      update_display_(false),
      smoothed_level_(0.0f),
      input_peak_(0.0f),
      was_arp_on_(false) {
    memset(adc_raw_values_, 0, sizeof(adc_raw_values_));
    for(int i = 0; i < 12; ++i) arp_led_ts_[i].store(0, std::memory_order_relaxed);
    buffers_[0] = ControlSnapshot{};
    buffers_[1] = ControlSnapshot{};
    audio_snap_ = ControlSnapshot{};
}

void ControlsManager::Init(float sample_rate) {
    arp_.Init(sample_rate);
    arp_.SetDirection(Arpeggiator::AsPlayed);
    arp_enabled_.store(false, std::memory_order_release);
    arp_clear_requested_.store(false, std::memory_order_release);
    read_idx_ = 0;
    write_idx_.store(0, std::memory_order_release);
    audio_snap_ = ControlSnapshot{};
}

void ControlsManager::SetArpEnabled(bool enabled) {
    arp_enabled_.store(enabled, std::memory_order_release);
    if(!enabled) arp_clear_requested_.store(true, std::memory_order_release);
}

bool ControlsManager::ConsumeArpClearRequest() {
    return arp_clear_requested_.exchange(false, std::memory_order_acq_rel);
}

void ControlsManager::UpdateControlSnapshot(const ControlSnapshot& snapshot) {
    uint8_t cur = write_idx_.load(std::memory_order_relaxed);
    uint8_t next = cur ^ 1;
    buffers_[next] = snapshot;
    write_idx_.store(next, std::memory_order_release);
}

void ControlsManager::SyncAudioControlSnapshot() {
    uint8_t w = write_idx_.load(std::memory_order_acquire);
    if(w != read_idx_) {
        read_idx_ = w;
        audio_snap_ = buffers_[read_idx_];
    }
}
