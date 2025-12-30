#ifndef HARDWARE_H
#define HARDWARE_H

#include "daisy_patch_sm.h"
#include "mpr121.h"
#include "util/CpuLoadMeter.h"

class HardwareManager {
public:
    HardwareManager();
    ~HardwareManager() = default;

    void Init();

    daisy::patch_sm::DaisyPatchSM& GetHardware() { return hw_; }
    kymatikos_hal::Mpr121& GetTouchSensor() { return touch_sensor_; }
    daisy::CpuLoadMeter& GetCpuMeter() { return cpu_meter_; }

    daisy::AnalogControl& GetCV5Knob() { return cv5_knob_; }
    daisy::AnalogControl& GetCV6Knob() { return cv6_knob_; }
    daisy::AnalogControl& GetCV7Knob() { return cv7_knob_; }
    daisy::AnalogControl& GetPitchKnob() { return pitch_knob_; }
    daisy::AnalogControl& GetArpPad() { return arp_pad_; }
    daisy::AnalogControl& GetPrevPad() { return prev_pad_; }
    daisy::AnalogControl& GetNextPad() { return next_pad_; }
    daisy::AnalogControl& GetModWheel() { return mod_wheel_; }

    // CV1-4 as button inputs (+4-5V = pressed)
    daisy::AnalogControl& GetCVBtn(int i) { return cv_btn_[i]; }
    bool IsCVBtnPressed(int i) { return cv_btn_[i].Value() > 0.85f; }

    daisy::GPIO* GetTouchLEDs() { return touch_leds_; }
    daisy::GPIO& GetTouchLED(int index) { return touch_leds_[index]; }
    void SetTouchLEDs(bool state);
    void SetGateOut2(bool state);
    void SetPitchCvVoltage(float volts);
    void SetPressureCvVoltage(float volts);

    bool& IsTouchSensorPresent() { return touch_sensor_present_; }
    float& GetSampleRate() { return sample_rate_; }

private:
    daisy::patch_sm::DaisyPatchSM hw_;
    kymatikos_hal::Mpr121 touch_sensor_;
    daisy::CpuLoadMeter cpu_meter_;

    daisy::AnalogControl cv5_knob_;
    daisy::AnalogControl cv6_knob_;
    daisy::AnalogControl cv7_knob_;
    daisy::AnalogControl pitch_knob_;
    daisy::AnalogControl arp_pad_;
    daisy::AnalogControl prev_pad_;
    daisy::AnalogControl next_pad_;
    daisy::AnalogControl mod_wheel_;
    daisy::AnalogControl cv_btn_[4];  // CV1-4 as buttons

    daisy::GPIO touch_leds_[12];
    daisy::GPIO gate_out2_;

    bool touch_sensor_present_;
    float sample_rate_;

    void InitHardware();
    void InitADCs();
    void InitTouchSensor();
    void InitLEDs();
    void InitGateOutputs();
};

#endif
