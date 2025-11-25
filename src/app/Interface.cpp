#include "Kymatikos.h"
#include "Arpeggiator.h"
#include "SynthStateStorage.h"
#include "AudioConfig.h"
#include <algorithm>
#include "hid/logger.h"

// --- Manager Includes ---
#include "HardwareManager.h"
#include "ControlsManager.h"
#include "AudioEngine.h"

// --- Namespace imports (local to this implementation file) ---
using namespace daisy;
using namespace daisysp;

// --- Global Manager Instances (replaces 48+ individual globals) ---
HardwareManager g_hardware;
ControlsManager g_controls;
AudioEngine g_audio_engine;

// Simple diagnostic blink: flashes the Daisy user LED 'count' times rapidly.
static void DebugBlink(int count)
{
    for(int i = 0; i < count; ++i)
    {
        g_hardware.GetHardware().SetLed(true);
        System::Delay(60);
        g_hardware.GetHardware().SetLed(false);
        System::Delay(60);
    }
}

// --- Application-Specific Initialization ---
// This handles initialization that is specific to the Kymatikos application,
// NOT hardware configuration (which is handled by HardwareManager).
static void InitializeApplication() {
    // Configure QSPI flash for memory-mapped mode
    SynthStateStorage::InitMemoryMapped();

    // Relocate the vector table to QSPI flash base address
    // This address (0x90040000) must match the QSPIFLASH ORIGIN in the linker script
    #if defined(__STM32H750xx_H) || defined(STM32H750xx) || defined(STM32H7XX)
        SCB->VTOR = 0x90040000UL;
    #endif
}

void InitializeSynth() {
    // Application-specific initialization (QSPI, VTOR)
    InitializeApplication();

    // Bring up USB logging before hardware init so early prints are visible.
    // Hardware initialization (Patch SM platform, ADCs, touch sensor, LEDs, CPU meter)
    g_hardware.Init();
    DebugBlink(1);

    // Controls initialization (arpeggiator and control state)
    g_controls.Init(g_hardware.GetSampleRate());
    DebugBlink(2);

    // Audio engine initialization (Clouds processor only)
    g_audio_engine.Init(&g_hardware.GetHardware());
    DebugBlink(3);

    g_controls.GetArpeggiator().SetNoteTriggerCallback([&](int pad_idx){
        g_controls.SetArpLEDTimestamp(11 - pad_idx, g_hardware.GetHardware().system.GetNow());
        RequestArpGatePulse();
        g_hardware.SetPitchCvVoltage(PadIndexToVoltage(pad_idx));
    });
    DebugBlink(4);

    g_controls.SetArpEnabled(false);

    g_hardware.GetHardware().StartLog(false); // Start log immediately (non-blocking)
    DebugBlink(5);

    g_hardware.GetHardware().StartAudio(AudioCallback);
    DebugBlink(6);

    g_hardware.GetHardware().PrintLine("Clouds Granular Processor - Ready");
    char settings[64];
    sprintf(settings, "Block: %d | SR: %d", BLOCK_SIZE, (int)g_hardware.GetSampleRate());
    g_hardware.GetHardware().PrintLine(settings);
    g_hardware.GetHardware().PrintLine("Keyboard-Controlled Granular Synthesis");
    g_hardware.GetHardware().PrintLine("----------------");
}

// --- User Interface Functions ---
void Bootload() {
    /*
        Bootloader entry via ADC combo (pads 8-10).
        Require the condition to be held for ~1 s to avoid false triggers
        from floating ADC inputs or brief noise at startup.
    */
    static uint16_t hold_cnt = 0;
    const uint16_t kHoldFrames = 500; // 500 * 2 ms ≈ 1 s

    // Skip bootloader combo for first 5 seconds after power-up to avoid
    // false triggers from floating ADC inputs while they settle.
    if(System::GetNow() < 5000)
        return;

    bool combo_pressed = (g_hardware.GetArpPad().GetRawFloat()   > 0.5f) &&
                         (g_hardware.GetPrevPad().GetRawFloat() > 0.5f) &&
                         (g_hardware.GetNextPad().GetRawFloat() > 0.5f);

    if(combo_pressed)
    {
        if(++hold_cnt >= kHoldFrames)
        {
            g_hardware.GetHardware().PrintLine("Entering Daisy DFU bootloader (ADC combo)…");
            System::Delay(100);
            // Jump to Daisy bootloader and keep it in DFU until a new
            // image is flashed. This avoids ending up in the STM ROM DFU
            // which cannot load QSPI apps.
            System::ResetToBootloader(System::BootloaderMode::DAISY_INFINITE_TIMEOUT);
        }
    }
    else
    {
        hold_cnt = 0; // reset counter when combo released
    }
}

void UpdateLED() {
    // LED heartbeat - blinks to indicate system is running
    bool led_on = (System::GetNow() % 1000) < 500;
    g_hardware.GetHardware().SetLed(led_on);
    if(!g_hardware.IsTouchSensorPresent()) {
        // Touch controller missing: blink all touch LEDs so they're not stuck on.
        g_hardware.SetTouchLEDs(led_on);
    }
}

void UpdateArpeggiatorToggle() {
    constexpr float kThreshOn  = 0.30f;
    constexpr float kThreshOff = 0.20f;
    static bool pressed = false;

    float pad = g_hardware.GetArpPad().Value();
    if (!pressed && pad > kThreshOn) {
        pressed = true;
        bool next = !g_controls.IsArpEnabled();
        g_controls.SetArpEnabled(next);
        if (next) {
            g_controls.GetArpeggiator().Init(g_hardware.GetSampleRate());
            g_controls.GetArpeggiator().SetDirection(Arpeggiator::AsPlayed);
        }
    } else if (pressed && pad < kThreshOff) {
        pressed = false;
    }
}

void UpdateEngineSelection() {
    // TODO: repurpose for Clouds mode selection
}

// Moved from AudioProcessor.cpp
void ProcessControls() {
    g_hardware.GetCV5Knob().Process();            // ADC 4
    g_hardware.GetCV6Knob().Process();            // ADC 5
    g_hardware.GetCV7Knob().Process();            // ADC 6
    g_hardware.GetPitchKnob().Process();          // ADC 7
    g_hardware.GetArpPad().Process();             // ADC 8: Arpeggiator Pad
    g_hardware.GetPrevPad().Process();            // ADC 9: Previous Pad
    g_hardware.GetNextPad().Process();            // ADC 10: Next Pad
    g_hardware.GetModWheel().Process();           // ADC 11: Mod Wheel Control

    // Read raw values for ALL 12 ADC channels
    auto& hw = g_hardware.GetHardware();
    for(int i = 0; i < 12; ++i) {
        g_controls.GetADCRawValues()[i] = hw.adc.GetFloat(i);
    }

    // Call the new engine selection function
    UpdateEngineSelection();
    UpdateArpeggiatorToggle(); // Call the new arp toggle function
}

// Moved from AudioProcessor.cpp
void ReadKnobValues() {
    ControlsManager::ControlSnapshot snapshot{};
    const float cv5_raw = g_hardware.GetCV5Knob().Value();                // ADC 4
    const float cv5 = daisysp::fclamp(cv5_raw, 0.0f, 1.0f);
    snapshot.position_knob = cv5_raw;

    const float cv6_raw = g_hardware.GetCV6Knob().Value();
    const float size = daisysp::fclamp(cv6_raw, 0.0f, 1.0f);
    snapshot.density_knob = cv6_raw;

    const float raw_cv7 = g_hardware.GetCV7Knob().Value();
    const float blend = daisysp::fclamp(1.0f - raw_cv7, 0.0f, 1.0f);
    snapshot.blend_knob = blend;

    constexpr float kReverbThresh = 0.4f;
    constexpr float kMaxBalance = 0.9f;
    const float feedback = daisysp::fclamp(blend, 0.0f, 0.33f);
    float reverb = (blend < kReverbThresh) ? 0.0f
                   : (blend - kReverbThresh) / (1.0f - kReverbThresh);
    reverb = daisysp::fclamp(reverb, 0.0f, kMaxBalance);
    const float dry_wet = daisysp::fclamp(blend, 0.0f, kMaxBalance);

    const float mod_wheel = g_hardware.GetModWheel().Value();
    snapshot.mod_wheel = mod_wheel;
    const float pitch = daisysp::fclamp((mod_wheel * 2.0f - 1.0f) * 12.0f, -12.0f, 12.0f);

    snapshot.clouds_position = cv5;
    snapshot.clouds_size = size;
    snapshot.clouds_density = blend;
    snapshot.clouds_texture = 0.5f;
    snapshot.clouds_feedback = feedback;
    snapshot.clouds_reverb = reverb;
    snapshot.clouds_dry_wet = dry_wet;
    snapshot.clouds_pitch = pitch;
    snapshot.pitch = g_hardware.GetPitchKnob().Value();

    g_controls.UpdateControlSnapshot(snapshot);
}
