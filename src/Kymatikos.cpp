#include "Kymatikos.h"
#include "Arpeggiator.h"
#include "settings.h"
#include <atomic>
#include <algorithm>

using namespace daisy;
using namespace daisysp;

HardwareManager g_hardware;
ControlsManager g_controls;

const float kArabicMaqamScale[12] = {
    0.0f, 2.0f, 4.0f, 7.0f, 9.0f, 12.0f,
    14.0f, 16.0f, 19.0f, 21.0f, 24.0f, 26.0f,
};

static const int kMprToLed[12] = {9, 8, 7, 6, 3, 4, 5, 2, 1, 0, 10, 11};
static std::atomic<uint32_t> g_gate_deadline{0};
static ControlsManager::ControlSnapshot g_last_snap{};

float PadIndexToVoltage(int pad_index) {
    if(pad_index < 0) return 0.0f;
    constexpr int kCenter = 6;
    constexpr float kBase = 2.5f;
    float offset = kArabicMaqamScale[pad_index] - kArabicMaqamScale[kCenter] - 12.0f;
    return kBase + (offset / 12.0f);
}

static float PadPosToVoltage(float pos) {
    pos = daisysp::fclamp(pos, 0.0f, 11.0f);
    int i0 = static_cast<int>(pos);
    int i1 = std::min(i0 + 1, 11);
    float frac = pos - static_cast<float>(i0);
    constexpr int kCenter = 6;
    constexpr float kBase = 2.5f;
    float p0 = kArabicMaqamScale[i0] - kArabicMaqamScale[kCenter] - 12.0f;
    float p1 = kArabicMaqamScale[i1] - kArabicMaqamScale[kCenter] - 12.0f;
    return kBase + ((p0 * (1.0f - frac) + p1 * frac) / 12.0f);
}

void RequestArpGatePulse() {
    uint32_t now = g_hardware.GetHardware().system.GetNow();
    g_gate_deadline.store(now + 10, std::memory_order_relaxed);
    g_hardware.SetGateOut2(true);
}

void InitializeSynth() {
    #if defined(__STM32H750xx_H) || defined(STM32H750xx) || defined(STM32H7XX)
        SCB->VTOR = 0x90040000UL;
    #endif

    g_hardware.Init();
    g_controls.Init(g_hardware.GetSampleRate());
    InitClouds(g_hardware.GetSampleRate());

    g_controls.GetArpeggiator().SetNoteTriggerCallback([&](int pad_idx){
        g_controls.SetArpLEDTimestamp(kMprToLed[pad_idx], g_hardware.GetHardware().system.GetNow());
        RequestArpGatePulse();
        g_hardware.SetPitchCvVoltage(PadIndexToVoltage(pad_idx));
    });

    g_controls.SetArpEnabled(false);
    g_hardware.GetHardware().StartLog(false);
    g_hardware.GetHardware().StartAudio(AudioCallback);
    g_hardware.GetHardware().PrintLine("Kymatikos Ready | Block:%d SR:%d", BLOCK_SIZE, (int)g_hardware.GetSampleRate());
}

void Bootload() {
    static uint32_t touch_start_time = 0;
    if(System::GetNow() > 5000) return;  // Only allow bootloader entry in first 5 seconds

    uint16_t touched = g_controls.GetCurrentTouchState();

    if(touched != 0) {
        if(touch_start_time == 0) {
            touch_start_time = System::GetNow();
        } else if(System::GetNow() - touch_start_time >= 500) {
            RTC->BKP0R = kBootloaderMagic;
            g_hardware.GetHardware().PrintLine("Entering DFU...");
            System::Delay(100);
            System::ResetToBootloader(System::BootloaderMode::DAISY_INFINITE_TIMEOUT);
        }
    } else {
        touch_start_time = 0;
    }
}

void UpdateLED() {
    bool on = (System::GetNow() % 1000) < 500;
    g_hardware.GetHardware().SetLed(on);
    if(!g_hardware.IsTouchSensorPresent()) g_hardware.SetTouchLEDs(on);
}

void UpdateArpeggiatorToggle() {
    static bool pressed = false;
    float pad = g_hardware.GetArpPad().Value();
    if (!pressed && pad > 0.30f) {
        pressed = true;
        bool next = !g_controls.IsArpEnabled();
        g_controls.SetArpEnabled(next);
        if (next) {
            g_controls.GetArpeggiator().Init(g_hardware.GetSampleRate());
            g_controls.GetArpeggiator().SetDirection(Arpeggiator::AsPlayed);
        }
    } else if (pressed && pad < 0.20f) {
        pressed = false;
    }
}

void ProcessControls() {
    g_hardware.GetCV5Knob().Process();
    g_hardware.GetCV6Knob().Process();
    g_hardware.GetCV7Knob().Process();
    g_hardware.GetFSR().Process();
    g_hardware.GetArpPad().Process();
    g_hardware.GetPrevPad().Process();
    g_hardware.GetNextPad().Process();
    g_hardware.GetModWheel().Process();
    for(int i = 0; i < 4; ++i) g_hardware.GetCVBtn(i).Process();

    auto& hw = g_hardware.GetHardware();
    for(int i = 0; i < 12; ++i) {
        g_controls.GetADCRawValues()[i] = hw.adc.GetFloat(i);
    }
    UpdateArpeggiatorToggle();
}

void ReadKnobValues() {
    ControlsManager::ControlSnapshot snap{};
    auto scale = [](float raw, float max) { return daisysp::fclamp(raw / max, 0.0f, 1.0f); };

    float cv5 = scale(g_hardware.GetCV5Knob().Value(), 0.50f);
    float cv6 = scale(g_hardware.GetCV6Knob().Value(), 0.50f);
    float cv7 = 1.0f - scale(g_hardware.GetCV7Knob().Value(), 0.50f);

    snap.clouds_position = cv5;
    snap.clouds_size = cv6;
    snap.clouds_density = cv7;
    snap.clouds_texture = 0.7f;
    snap.clouds_feedback = 0.62f;
    snap.clouds_reverb = 0.8f;
    snap.clouds_dry_wet = 0.9f;
    snap.clouds_pitch = 0.0f;
    snap.master_volume = 1.0f;

    g_controls.UpdateControlSnapshot(snap);
    g_last_snap = snap;
    g_controls.GetArpeggiator().SetMainTempoFromKnob(cv5);
}

void UpdateDisplay() {
    if (!g_controls.ShouldUpdateDisplay()) return;
    
    int cpu_avg = static_cast<int>(g_hardware.GetCpuMeter().GetAvgCpuLoad() * 100.0f);
    int cpu_max = static_cast<int>(g_hardware.GetCpuMeter().GetMaxCpuLoad() * 100.0f);
    const auto& snap = g_last_snap;
    int pressure_mv = static_cast<int>(g_controls.GetTouchCVValue() * 5000.0f);
    
    g_hardware.GetHardware().PrintLine(
        "CPU:%d/%d In:%d Touch:%03X Pos:%d Size:%d Dens:%d P:%dmV",
        cpu_avg, cpu_max,
        static_cast<int>(g_controls.GetInputPeakLevel() * 1000.0f),
        g_controls.GetCurrentTouchState(),
        static_cast<int>(snap.clouds_position * 100),
        static_cast<int>(snap.clouds_size * 100),
        static_cast<int>(snap.clouds_density * 100),
        pressure_mv);
    
    g_controls.SetUpdateDisplay(false);
}

void PollTouchSensor() {
    static bool last_gate = false;
    static int last_pad = -1, prev_pad = -1;
    static float pressure_env = 0.0f, slider_pos = 6.0f, prev_slider = 6.0f;
    static float vib_depth = 0.0f, vib_phase = 0.0f;
    
    uint16_t prev_touch = g_controls.GetCurrentTouchState();
    uint32_t now = g_hardware.GetHardware().system.GetNow();
    bool gate_active = now < g_gate_deadline.load(std::memory_order_relaxed);

    if(!g_hardware.IsTouchSensorPresent()) {
        if(gate_active != last_gate) { g_hardware.SetGateOut2(gate_active); last_gate = gate_active; }
        return;
    }
    
    if(g_hardware.GetTouchSensor().HasError()) {
        g_hardware.GetTouchSensor().ClearError();
        kymatikos_hal::Mpr121::Config cfg; cfg.Defaults();
        g_hardware.GetTouchSensor().Init(cfg);
        g_hardware.GetTouchSensor().SetThresholds(6, 3);
    }
    
    uint16_t touched = g_hardware.GetTouchSensor().Touched();
    
    if(touched != 0 && prev_touch == 0) { RequestArpGatePulse(); gate_active = true; }
    if(gate_active != last_gate) { g_hardware.SetGateOut2(gate_active); last_gate = gate_active; }

    if(touched != 0) {
        for(int i = 11; i >= 0; --i) if(touched & (1 << i)) { last_pad = i; break; }
    }
    if(last_pad != prev_pad) { g_hardware.SetPitchCvVoltage(PadIndexToVoltage(last_pad)); prev_pad = last_pad; }

    g_controls.SetCurrentTouchState(touched);

    bool arp_on = g_controls.GetArpeggiator().IsActive();
    auto* leds = g_hardware.GetTouchLEDs();
    for(int i = 0; i < 12; ++i) {
        int idx = kMprToLed[i];
        bool pad_on = (touched & (1 << i)) != 0;
        bool blink = (now - g_controls.GetArpLEDTimestamp(idx)) < ControlsManager::ARP_LED_DURATION_MS;
        leds[idx].Write(arp_on ? blink : (pad_on || blink));
    }

    // FSR pressure from CV8 -> CV OUT 2
    // Patch SM reads 0V as 0.5, +5V as 0.0, so shift and scale for 0-5V input range
    float fsr_raw = g_hardware.GetFSR().Value();
    float fsr_val = daisysp::fmax(0.0f, (0.5f - fsr_raw) * 2.0f);
    g_hardware.SetPressureCvVoltage(fsr_val * 5.0f);

    if (touched == 0) {
        pressure_env *= 0.95f;
        g_controls.SetTouchCVValue(pressure_env);
        return;
    }

    int16_t max_dev = 0;
    float wpos = 0.0f, wsum = 0.0f;
    for (int i = 0; i < 12; i++) {
        if (touched & (1 << i)) {
            int16_t dev = g_hardware.GetTouchSensor().GetBaselineDeviation(i);
            if (dev > max_dev) max_dev = dev;
            if (dev > 0) { wpos += dev * i; wsum += dev; }
        }
    }

    if(wsum > 1e-3f) slider_pos = slider_pos * 0.9f + (wpos / wsum) * 0.1f;
    float slide_delta = fabsf(slider_pos - prev_slider);
    prev_slider = slider_pos;

    float norm = sqrtf(daisysp::fclamp(max_dev / 140.0f, 0.0f, 1.0f));
    float prev_cv = g_controls.GetTouchCVValue();
    float change = fabsf(norm - prev_cv);
    float smooth = daisysp::fmax(0.5f, 0.95f - change * 2.0f);
    pressure_env = prev_cv * smooth + norm * (1.0f - smooth);
    g_controls.SetTouchCVValue(pressure_env);

    constexpr float kTwoPi = 6.283185307f;
    vib_depth = vib_depth * 0.75f + daisysp::fmin(change * 1.2f + slide_delta, 1.0f) * 0.25f;
    vib_phase += kTwoPi * 6.0f * 0.001f;
    if(vib_phase > kTwoPi) vib_phase -= kTwoPi;

    float pitch_v = PadPosToVoltage(slider_pos) + sinf(vib_phase) * vib_depth / 12.0f;
    g_hardware.SetPitchCvVoltage(daisysp::fclamp(pitch_v, 0.0f, 5.0f));
}

int main(void) {
if(RTC->BKP0R == kBootloaderMagic) RTC->BKP0R = 0;
    InitializeSynth();

    uint32_t lastPoll = 0, lastUI = 0;
    while (1) {
        uint32_t now = g_hardware.GetHardware().system.GetNow();
        if (now - lastUI >= 1) { lastUI = now; ProcessControls(); ReadKnobValues(); }
        UpdateLED();
        Bootload();
        //UpdateDisplay();
        if (now - lastPoll >= 5) { lastPoll = now; PollTouchSensor(); }
    }
}
