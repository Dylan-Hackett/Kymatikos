#include "Kymatikos.h"

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
    float pitch_offset = kArabicMaqamScale[pad_index] - kArabicMaqamScale[kCenterPad];
    return kBaseVoltage + (pitch_offset / 12.0f);
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
        pos += snprintf(msg + pos, sizeof(msg) - pos, "Touch: %03X (%s)\n",
                        g_controls.GetCurrentTouchState(),
                        g_hardware.IsTouchSensorPresent() ? "OK" : "MISS");

        const auto& control_snapshot = g_controls.GetLatestControlSnapshot();
        int cv5 = static_cast<int>(control_snapshot.position_knob * 1000.0f);
        int cv6 = static_cast<int>(control_snapshot.density_knob * 1000.0f);
        int cv7 = static_cast<int>(control_snapshot.blend_knob * 1000.0f);
        int raw7 = static_cast<int>(g_hardware.GetMorphKnob().GetRawFloat() * 1000.0f);
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

        g_controls.ShouldUpdateDisplay() = false;
    }
}

// Poll the touch sensor and update shared variables
void PollTouchSensor() {
    static bool last_gate_state = false;
    static int last_pad_index = -1;
    if(!g_hardware.IsTouchSensorPresent()) {
        // Touch sensor unavailable – nothing to poll
        if(last_gate_state) {
            g_hardware.SetGateOut2(false);
            last_gate_state = false;
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
    bool gate_state = touched != 0;
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
    g_hardware.SetPitchCvVoltage(PadIndexToVoltage(last_pad_index));

    g_controls.GetCurrentTouchState() = touched;

    // Update touch pad LEDs with touch and ARP blink
    uint32_t now = g_hardware.GetHardware().system.GetNow();
    bool arp_on = g_controls.GetArpeggiator().IsActive();
    auto* touch_leds = g_hardware.GetTouchLEDs();
    for(int i = 0; i < 12; ++i) {
        int ledIdx = 11 - i;  // pad i maps to LED[11-i]
        bool padTouched = (touched & (1 << i)) != 0;
        bool blink     = (now - g_controls.GetArpLEDTimestamps()[ledIdx]) < ControlsManager::ARP_LED_DURATION_MS;
        bool ledState  = arp_on ? blink : (padTouched || blink);
        touch_leds[ledIdx].Write(ledState);
    }

    if (touched == 0) {

        g_controls.GetTouchCVValue() = g_controls.GetTouchCVValue() * 0.95f;
        return;
    }

    float total_deviation = 0.0f;
    int touched_count = 0;

    // Iterate through all pads
    for (int i = 0; i < 12; i++) {
        if (touched & (1 << i)) {
            int16_t deviation = g_hardware.GetTouchSensor().GetBaselineDeviation(i);
            total_deviation += deviation;
            touched_count++;
        }
    }

    float average_deviation = 0.0f;
    if (touched_count > 0) {
        average_deviation = total_deviation / touched_count;
    }

    // Normalize to 0.0-1.0 range with adjustable sensitivity
    // Consider recalibrating sensitivity if needed for average pressure
    float sensitivity = 150.0f;
    float normalized_value = daisysp::fmax(0.0f, daisysp::fmin(1.0f, average_deviation / sensitivity));

    // Apply curve for better control response (squared curve feels more natural)
    normalized_value = normalized_value * normalized_value;

    // Remove the position component - control is now based purely on average pressure
    // float position_value = highest_pad / 11.0f; // Removed
    // float position_weight = 0.7f;              // Removed
    // float combined_value = position_value * position_weight + normalized_value * (1.0f - position_weight); // Replaced
    float combined_value = normalized_value; // Use normalized average pressure directly

    // Apply adaptive smoothing - more smoothing for small changes, less for big changes
    float change = fabsf(combined_value - g_controls.GetTouchCVValue());
    float smoothing = daisysp::fmax(0.5f, 0.95f - change * 2.0f); // 0.5-0.95 smoothing range

    // Update the shared volatile variable
    g_controls.GetTouchCVValue() = g_controls.GetTouchCVValue() * smoothing + combined_value * (1.0f - smoothing);
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

            static uint32_t last_knob_log = 0;
            const auto& control_snapshot = g_controls.GetLatestControlSnapshot();
            if (now - last_knob_log >= 200) {
                last_knob_log = now;
                int cv5 = static_cast<int>(control_snapshot.position_knob * 1000.0f);
                int cv6 = static_cast<int>(control_snapshot.density_knob * 1000.0f);
                int cv7 = static_cast<int>(control_snapshot.blend_knob * 1000.0f);
                int raw7 = static_cast<int>(g_hardware.GetMorphKnob().GetRawFloat() * 1000.0f);
                int pos_val = static_cast<int>(control_snapshot.clouds_position * 1000.0f);
                int size_val = static_cast<int>(control_snapshot.clouds_size * 1000.0f);
                int dens_val = static_cast<int>(control_snapshot.clouds_density * 1000.0f);
                int text_val = static_cast<int>(control_snapshot.clouds_texture * 1000.0f);
                int fdb_val = static_cast<int>(control_snapshot.clouds_feedback * 1000.0f);
                int rev_val = static_cast<int>(control_snapshot.clouds_reverb * 1000.0f);
                g_hardware.GetHardware().PrintLine("CV5:%03d CV6:%03d CV7:%03d Raw7:%03d | Pos:%03d Size:%03d Dens:%03d Text:%03d Fdbk:%03d Rev:%03d",
                                                   cv5,
                                                   cv6,
                                                   cv7,
                                                   raw7,
                                                   pos_val,
                                                   size_val,
                                                   dens_val,
                                                   text_val,
                                                   fdb_val,
                                                   rev_val);
            }

            // Arpeggiator tempo control
            if (g_controls.IsArpEnabled()) {
                g_controls.GetArpeggiator().SetMainTempoFromKnob(control_snapshot.delay_time);
            }

            // Apply touch CV modulation to morph parameter
            // Morph modulation now handled in audio thread via control snapshots
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
