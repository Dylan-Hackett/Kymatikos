#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include "daisy_patch_sm.h"
#include "mpr121_daisy.h"
#include "util/CpuLoadMeter.h"

// NOTE: using namespace directives removed from header to avoid namespace pollution
// Implementation file (.cpp) should add using namespace as needed locally

/**
 * HardwareManager encapsulates all hardware-related state:
 * - Daisy Patch SM hardware
 * - Touch sensor (MPR121)
 * - 12 ADC controls (knobs/pads)
 * - 12 LED GPIOs
 * - CPU load meter
 *
 * This eliminates 29 global variables and provides a single
 * point of hardware initialization and access.
 */
class HardwareManager {
public:
    HardwareManager();
    ~HardwareManager() = default;

    // Initialize all hardware
    void Init();

    // Hardware access
    daisy::patch_sm::DaisyPatchSM& GetHardware() { return hw_; }
    kymatikos_hal::Mpr121& GetTouchSensor() { return touch_sensor_; }
    daisy::CpuLoadMeter& GetCpuMeter() { return cpu_meter_; }

    // ADC Control access
    daisy::AnalogControl& GetDelayTimeKnob() { return delay_time_knob_; }
    daisy::AnalogControl& GetDelayMixFeedbackKnob() { return delay_mix_feedback_knob_; }
    daisy::AnalogControl& GetEnvReleaseKnob() { return env_release_knob_; }
    daisy::AnalogControl& GetEnvAttackKnob() { return env_attack_knob_; }
    daisy::AnalogControl& GetTimbreKnob() { return timbre_knob_; }
    daisy::AnalogControl& GetHarmonicsKnob() { return harmonics_knob_; }
    daisy::AnalogControl& GetMorphKnob() { return morph_knob_; }
    daisy::AnalogControl& GetPitchKnob() { return pitch_knob_; }
    daisy::AnalogControl& GetArpPad() { return arp_pad_; }
    daisy::AnalogControl& GetModelPrevPad() { return model_prev_pad_; }
    daisy::AnalogControl& GetModelNextPad() { return model_next_pad_; }
    daisy::AnalogControl& GetModWheel() { return mod_wheel_; }

    // LED access
    daisy::GPIO* GetTouchLEDs() { return touch_leds_; }
    daisy::GPIO& GetTouchLED(int index) { return touch_leds_[index]; }
    void SetTouchLEDs(bool state);
    void SetGateOut2(bool state);
    void SetPitchCvVoltage(float volts);

    // Touch sensor state (returns reference for backward compatibility with assignment)
    bool& IsTouchSensorPresent() { return touch_sensor_present_; }
    void SetTouchSensorPresent(bool present) { touch_sensor_present_ = present; }

    // Sample rate (returns reference for backward compatibility with assignment)
    float& GetSampleRate() { return sample_rate_; }

private:
    // Hardware
    daisy::patch_sm::DaisyPatchSM hw_;
    kymatikos_hal::Mpr121 touch_sensor_;
    daisy::CpuLoadMeter cpu_meter_;

    // 12 ADC Controls
    daisy::AnalogControl delay_time_knob_;        // ADC 0 (Pin 15)
    daisy::AnalogControl delay_mix_feedback_knob_; // ADC 1 (Pin 16)
    daisy::AnalogControl env_release_knob_;       // ADC 2 (Pin 17)
    daisy::AnalogControl env_attack_knob_;        // ADC 3 (Pin 18)
    daisy::AnalogControl timbre_knob_;            // ADC 4 (Pin 20)
    daisy::AnalogControl harmonics_knob_;         // ADC 5 (Pin 21)
    daisy::AnalogControl morph_knob_;             // ADC 6 (Pin 22)
    daisy::AnalogControl pitch_knob_;             // ADC 7 (Pin 23)
    daisy::AnalogControl arp_pad_;                // ADC 8 (Pin 23)
    daisy::AnalogControl model_prev_pad_;         // ADC 9 (Pin 24)
    daisy::AnalogControl model_next_pad_;         // ADC 10 (Pin 25)
    daisy::AnalogControl mod_wheel_;              // ADC 11 (Pin 28)

    // 12 Touch LEDs
    daisy::GPIO touch_leds_[12];
    daisy::GPIO gate_out2_;

    // State
    bool touch_sensor_present_;
    float sample_rate_;

    // Private initialization helpers
    void InitHardware();
    void InitADCs();
    void InitTouchSensor();
    void InitLEDs();
    void InitGateOutputs();
};

#endif // HARDWARE_MANAGER_H
