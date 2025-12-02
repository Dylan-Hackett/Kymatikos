#include "Kymatikos.h"
#include <atomic>

// --- Namespace imports (local to this implementation file) ---
using namespace daisy;
using namespace daisysp;

const float kArabicMaqamScale[12] = {
    0.0f,
    2.0f,
    4.0f,
    7.0f,
    9.0f,
    12.0f,
    14.0f,
    16.0f,
    19.0f,
    21.0f,
    24.0f,
    26.0f,
};

float PadIndexToVoltage(int pad_index)
{
    if(pad_index < 0)
        return 0.0f;
    constexpr int kCenterPad = 6;
    constexpr float kBaseVoltage = 2.5f;
    float pitch_offset = kArabicMaqamScale[pad_index] - kArabicMaqamScale[kCenterPad] - 12.0f;
    return kBaseVoltage + (pitch_offset / 12.0f);
}

// Arp gate pulse deadline (ms since boot) for analog gate output
static std::atomic<uint32_t> g_gate_deadline_ms{0};
static constexpr uint32_t kGatePulseMs = 10;

// Called from arp trigger callback to request a short gate pulse
void RequestArpGatePulse() {
    uint32_t now = g_hardware.GetHardware().system.GetNow();
    g_gate_deadline_ms.store(now + kGatePulseMs, std::memory_order_relaxed);
    g_hardware.SetGateOut2(true);
}

// Volatile variables now managed by ControlsManager (see g_controls)

void UpdateDisplay() {
    if (g_controls.ShouldUpdateDisplay()) {
        // Build stats message in a smaller buffer
        char msg[512];
        int pos = 0;

        // cpu load average/max
        float avg_cpu_load = g_hardware.GetCpuMeter().GetAvgCpuLoad();
        int cpu_avg = static_cast<int>(avg_cpu_load * 100.0f);
        cpu_avg = cpu_avg < 0 ? 0 : (cpu_avg > 100 ? 100 : cpu_avg);
        float max_cpu_load = g_hardware.GetCpuMeter().GetMaxCpuLoad();
        int cpu_max = static_cast<int>(max_cpu_load * 100.0f);
        cpu_max = cpu_max < 0 ? 0 : (cpu_max > 100 ? 100 : cpu_max);
        pos += snprintf(msg + pos, sizeof(msg) - pos, "cpu : %d/%d\n", cpu_avg, cpu_max);

        // Engine Info
        int current_engine_idx = g_controls.GetCurrentEngineIndex();
        pos += snprintf(msg + pos, sizeof(msg) - pos, "Engine: %d (%s)\n", current_engine_idx, (current_engine_idx <= 3) ? "Poly-4" : "Mono");
        pos += snprintf(msg + pos, sizeof(msg) - pos, "InLvl: %d\n", static_cast<int>(g_controls.GetInputPeakLevel() * 1000.0f));
        pos += snprintf(msg + pos, sizeof(msg) - pos, "Touch: %03X (%s) Pres:%d\n",
                        g_controls.GetCurrentTouchState(),
                        g_hardware.IsTouchSensorPresent() ? "OK" : "MISS",
                        static_cast<int>(g_controls.GetTouchCVValue() * 1000.0f));

        const auto& control_snapshot = g_controls.GetLatestControlSnapshot();
        int cv5 = static_cast<int>(control_snapshot.position_knob * 1000.0f);
        int cv6 = static_cast<int>(control_snapshot.density_knob * 1000.0f);
        int cv7 = static_cast<int>(control_snapshot.blend_knob * 1000.0f);
        int raw7 = static_cast<int>(g_hardware.GetCV7Knob().GetRawFloat() * 1000.0f);
        int pos_val = static_cast<int>(control_snapshot.clouds_position * 1000.0f);
        int size_val = static_cast<int>(control_snapshot.clouds_size * 1000.0f);
        int density_val = static_cast<int>(control_snapshot.clouds_density * 1000.0f);
        int texture_val = static_cast<int>(control_snapshot.clouds_texture * 1000.0f);
        int feedback_val = static_cast<int>(control_snapshot.clouds_feedback * 1000.0f);
        int reverb_val = static_cast<int>(control_snapshot.clouds_reverb * 1000.0f);
        pos += snprintf(msg + pos,
                        sizeof(msg) - pos,
                        "CV5:%03d CV6:%03d CV7:%03d Raw7:%03d\nPos:%03d Size:%03d Dens:%03d Text:%03d Fdbk:%03d Rev:%03d\n",
                        cv5,
                        cv6,
                        cv7,
                        raw7,
                        pos_val,
                        size_val,
                        density_val,
                        texture_val,
                        feedback_val,
                        reverb_val);

        // Only show ADC values 8-11
        pos += snprintf(msg + pos, sizeof(msg) - pos, "ADC Values (8-11):\n");
        for (int i = 8; i < 12; ++i) {
            pos += snprintf(msg + pos, sizeof(msg) - pos, "[%d]: %d\n", i, static_cast<int>(g_controls.GetADCRawValues()[i] * 1000));
            if(pos >= (int)sizeof(msg) - 20) break; // safety margin
        }

        // Separator
        pos += snprintf(msg + pos, sizeof(msg) - pos, "--------");

        // Print once with a single call to avoid throttling
        g_hardware.GetHardware().PrintLine("%s", msg);

        g_controls.SetUpdateDisplay(false);
    }
}

// Poll the touch sensor and update shared variables
void PollTouchSensor() {
    static bool last_gate_state = false;
    static int last_pad_index = -1;
    uint16_t prev_touch_state = g_controls.GetCurrentTouchState();
    uint32_t now = g_hardware.GetHardware().system.GetNow();
    bool gate_pulse_active = now < g_gate_deadline_ms.load(std::memory_order_relaxed);

    if(!g_hardware.IsTouchSensorPresent()) {
        // Touch sensor unavailable – nothing to poll
        if(gate_pulse_active != last_gate_state) {
            g_hardware.SetGateOut2(gate_pulse_active);
            last_gate_state = gate_pulse_active;
        }
        last_pad_index = -1;
        return;
    }
    // Recover from any I2C errors on the touch sensor
    if(g_hardware.GetTouchSensor().HasError()) {
        g_hardware.GetTouchSensor().ClearError();
        kymatikos_hal::Mpr121::Config cfg;
        cfg.Defaults();
        g_hardware.GetTouchSensor().Init(cfg);
        g_hardware.GetTouchSensor().SetThresholds(6, 3);
    }
    uint16_t touched = g_hardware.GetTouchSensor().Touched();
    
    // MPR touch pad to LED index mapping (used in multiple places)
    static const int kMprToLed[12] = {9, 8, 7, 6, 3, 4, 5, 2, 1, 0, 10, 11};
    
    // Diagnostic: print which pad was just pressed and which LED it maps to
    uint16_t newly_pressed = touched & ~prev_touch_state;
    if(newly_pressed != 0) {
        for(int i = 0; i < 12; ++i) {
            if(newly_pressed & (1 << i)) {
                g_hardware.GetHardware().PrintLine("PAD: MPR bit %d -> LED index %d", i, kMprToLed[i]);
            }
        }
    }
    
    // On a new touch, fire a gate pulse
    if(touched != 0 && prev_touch_state == 0) {
        RequestArpGatePulse();
        gate_pulse_active = true;
    }
    bool gate_state = gate_pulse_active;
    if(gate_state != last_gate_state) {
        g_hardware.SetGateOut2(gate_state);
        last_gate_state = gate_state;
    }

    if(touched != 0) {
        for(int i = 11; i >= 0; --i) {
            if(touched & (1 << i)) {
                last_pad_index = i;
                break;
            }
        }
    }
    // Drive pitch CV from touch when arp is inactive; only update on change to reduce DAC jitter
    static int prev_pad_index = -1;
    if(!g_controls.IsArpEnabled() || !g_controls.GetArpeggiator().IsActive()) {
        if(last_pad_index != prev_pad_index) {
            g_hardware.SetPitchCvVoltage(PadIndexToVoltage(last_pad_index));
            prev_pad_index = last_pad_index;
        }
    }

    g_controls.SetCurrentTouchState(touched);

    // Update touch pad LEDs with touch and ARP blink
    bool arp_on = g_controls.GetArpeggiator().IsActive();
    auto* touch_leds = g_hardware.GetTouchLEDs();
    for(int i = 0; i < 12; ++i) {
        int ledIdx = kMprToLed[i];
        bool padTouched = (touched & (1 << i)) != 0;
        bool blink = (now - g_controls.GetArpLEDTimestamp(ledIdx)) < ControlsManager::ARP_LED_DURATION_MS;
        bool ledState = arp_on ? blink : (padTouched || blink);
        touch_leds[ledIdx].Write(ledState);
    }

    if (touched == 0) {
        float decayed = g_controls.GetTouchCVValue() * 0.95f;
        g_controls.SetTouchCVValue(decayed);
        g_hardware.SetPressureCvVoltage(0.0f); // 0V when not touching
        return;
    }

    int16_t max_deviation = 0;

    // Find the maximum deviation from all touched pads
    for (int i = 0; i < 12; i++) {
        if (touched & (1 << i)) {
            int16_t deviation = g_hardware.GetTouchSensor().GetBaselineDeviation(i);
            if (deviation > max_deviation) {
                max_deviation = deviation;
            }
        }
    }

    // Normalize to 0.0-1.0 range using max deviation
    float sensitivity = 80.0f;
    float normalized_value = daisysp::fmax(0.0f, daisysp::fmin(1.0f, static_cast<float>(max_deviation) / sensitivity));
    // Gentler curve for better low-pressure response
    normalized_value = sqrtf(normalized_value);

    float prev_cv = g_controls.GetTouchCVValue();
    float change = fabsf(normalized_value - prev_cv);
    float smoothing = daisysp::fmax(0.5f, 0.95f - change * 2.0f);
    float new_cv = prev_cv * smoothing + normalized_value * (1.0f - smoothing);
    g_controls.SetTouchCVValue(new_cv);
    
    // Output pressure to CV OUT 2 (0-5V)
    g_hardware.SetPressureCvVoltage(new_cv * 5.0f);
    
    // Debug: show max deviation
    static uint32_t last_dbg = 0;
    if (now - last_dbg > 200) {
        last_dbg = now;
        g_hardware.GetHardware().PrintLine("DEV:%d CV:%dmV", 
            max_deviation,
            static_cast<int>(new_cv * 5000.0f));
    }
}

int main(void) {
    InitializeSynth();
    g_hardware.GetHardware().PrintLine("Kymatikos booted.");

    uint32_t lastPoll = g_hardware.GetHardware().system.GetNow();  // Track last poll time
    uint32_t lastUIUpdate = g_hardware.GetHardware().system.GetNow();  // Track last UI processing
    uint32_t lastHeartbeat = lastPoll;
    bool heartbeat_state = false;

    // Main Loop
    while (1) {
        uint32_t now = g_hardware.GetHardware().system.GetNow();

        // UI processing at 1ms intervals (1kHz) - moved from audio ISR for real-time safety
        if (now - lastUIUpdate >= 1) {
            lastUIUpdate = now;
            ProcessControls();
            ReadKnobValues();
        }

        UpdateLED();

        // Check bootloader condition via ADC touch pads
        Bootload();

        UpdateDisplay();
        g_audio_engine.GetCloudsProcessor().Prepare();

        // Poll touch sensor every 5 ms (200 Hz)
        if (now - lastPoll >= 5) {
            lastPoll = now;
            PollTouchSensor();
        }

        // Heartbeat indicator to verify firmware is running
        if (now - lastHeartbeat >= 500) {
            lastHeartbeat = now;
            heartbeat_state = !heartbeat_state;
            g_hardware.GetHardware().SetLed(heartbeat_state);
        }

        // Yield for system tasks, adjusted for polling interval
        System::Delay(1); // Shorter delay to allow more frequent polling checks
    }

    return 0;
}
