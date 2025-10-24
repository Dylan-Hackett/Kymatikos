#include "Kymatikos.h"

// --- Namespace imports (local to this implementation file) ---
using namespace daisy;
using namespace daisysp;
using namespace stmlib;

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
    if(!g_hardware.IsTouchSensorPresent()) {
        // Touch sensor unavailable – nothing to poll
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

    uint32_t lastPoll = g_hardware.GetHardware().system.GetNow();  // Track last poll time
    uint32_t lastUIUpdate = g_hardware.GetHardware().system.GetNow();  // Track last UI processing

    // Main Loop
    while (1) {
        uint32_t now = g_hardware.GetHardware().system.GetNow();

        // UI processing at 1ms intervals (1kHz) - moved from audio ISR for real-time safety
        if (now - lastUIUpdate >= 1) {
            lastUIUpdate = now;
            ProcessControls();
            ReadKnobValues();

            // Arpeggiator tempo control
            if (g_controls.IsArpEnabled()) {
                const auto& controls = g_controls.GetLatestControlSnapshot();
                g_controls.GetArpeggiator().SetMainTempoFromKnob(controls.delay_time);
            }

            // Apply touch CV modulation to morph parameter
            // Morph modulation now handled in audio thread via control snapshots
        }

        UpdateLED();

        // Check bootloader condition via ADC touch pads
        Bootload();

        UpdateDisplay();

        // Poll touch sensor every 5 ms (200 Hz)
        if (now - lastPoll >= 5) {
            lastPoll = now;
            PollTouchSensor();
        }

        // Yield for system tasks, adjusted for polling interval
        System::Delay(1); // Shorter delay to allow more frequent polling checks
    }

    return 0;
} 
