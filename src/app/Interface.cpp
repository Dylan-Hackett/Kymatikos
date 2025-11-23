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

    // Setup arpeggiator callback for LED feedback
    // Note: Arpeggiator is now used to modulate Clouds parameters rhythmically
    // rather than triggering voices
    g_controls.GetArpeggiator().SetNoteTriggerCallback([&](int pad_idx){
        // Update LED timestamps for visual feedback
        g_controls.GetArpLEDTimestamps()[11 - pad_idx] = g_hardware.GetHardware().system.GetNow();
        // TODO: Could modulate Clouds parameters here based on arp triggers
    });
    DebugBlink(4);

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
                         (g_hardware.GetModelPrevPad().GetRawFloat() > 0.5f) &&
                         (g_hardware.GetModelNextPad().GetRawFloat() > 0.5f);

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

// Function for Arpeggiator Toggle Pad
void UpdateArpeggiatorToggle() {
    constexpr float kOnThreshold  = 0.30f;   // more sensitive press detection
    constexpr float kOffThreshold = 0.20f;   // lower release threshold for fast reset
    static bool pad_pressed = false;          // debounced pad pressed state

    float pad_read = g_hardware.GetArpPad().Value();

    // Detect state transitions with hysteresis
    if(!pad_pressed && pad_read > kOnThreshold)
    {
        pad_pressed = true;
        // Rising edge detected -> toggle arp
        bool new_state = !g_controls.IsArpEnabled();
        g_controls.SetArpEnabled(new_state);
        if(new_state)
        {
            g_controls.GetArpeggiator().Init(g_hardware.GetSampleRate());          // restart timing
            g_controls.GetArpeggiator().SetNoteTriggerCallback([&](int pad_idx){
                // Update LED timestamps for visual feedback
                g_controls.GetArpLEDTimestamps()[11 - pad_idx] = g_hardware.GetHardware().system.GetNow();
            });
            g_controls.GetArpeggiator().SetDirection(Arpeggiator::AsPlayed); // Set default direction to AsPlayed
        }
    }
    else if(pad_pressed && pad_read < kOffThreshold)
    {
        // Consider pad released only when it falls well below off threshold
        pad_pressed = false;
    }
}

// Engine selection (simplified - no Plaits engines, can be repurposed for Clouds modes)
void UpdateEngineSelection() {
    // TODO: Could be repurposed to select different Clouds playback modes
    // (Granular, Pitch Shifter, Looping Delay, Spectral)
    // For now, this function is a no-op
}

// Moved from AudioProcessor.cpp
void ProcessControls() {
    g_hardware.GetDelayTimeKnob().Process();        // ADC 0
    g_hardware.GetEnvReleaseKnob().Process();       // ADC 2
    g_hardware.GetEnvAttackKnob().Process();        // ADC 3
    g_hardware.GetTimbreKnob().Process();            // ADC 4
    g_hardware.GetHarmonicsKnob().Process();         // ADC 5
    g_hardware.GetMorphKnob().Process();             // ADC 6
    g_hardware.GetPitchKnob().Process();             // ADC 7
    // Process the remaining ADC-based controls
    g_hardware.GetDelayMixFeedbackKnob().Process(); // ADC 1
    g_hardware.GetArpPad().Process();            // Process ADC 8: Arpeggiator Toggle Pad
    g_hardware.GetModelPrevPad().Process();     // Process ADC 9: Model Select Previous Pad
    g_hardware.GetModelNextPad().Process();     // Process ADC 10: Model Select Next Pad
    g_hardware.GetModWheel().Process();          // Process ADC 11: Mod Wheel Control

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
    snapshot.delay_time = g_hardware.GetDelayTimeKnob().Value();                 // ADC 0
    snapshot.delay_mix_feedback = g_hardware.GetDelayMixFeedbackKnob().Value(); // ADC 1
    snapshot.env_release = g_hardware.GetEnvReleaseKnob().Value();              // ADC 2
    snapshot.env_attack = g_hardware.GetEnvAttackKnob().Value();                // ADC 3
    const float cv5_raw = g_hardware.GetTimbreKnob().Value();                // ADC 4
    const float cv5 = daisysp::fclamp(cv5_raw, 0.0f, 1.0f);
    float clouds_position = cv5;
    snapshot.position_knob = cv5_raw;

    const float cv6_raw = g_hardware.GetHarmonicsKnob().Value();              // ADC 5
    const float cv6 = daisysp::fclamp(cv6_raw, 0.0f, 1.0f);
    float clouds_density = cv6;
    float clouds_texture = cv6;
    float clouds_size    = cv6;
    snapshot.density_knob = cv6_raw;

    const float raw_blend = g_hardware.GetMorphKnob().Value();                  // ADC 6
    const float blend_value = daisysp::fclamp(1.0f - raw_blend, 0.0f, 1.0f);
    float clouds_feedback = daisysp::fclamp(blend_value, 0.0f, 0.25f);
    float clouds_reverb = blend_value >= 0.7f ? 1.0f : (blend_value / 0.7f);
    float clouds_dry_wet = blend_value;
    snapshot.blend_knob = blend_value;

    const float mod_wheel = g_hardware.GetModWheel().Value();                      // ADC 11
    snapshot.mod_wheel = mod_wheel;
    float clouds_pitch = daisysp::fclamp((mod_wheel * 2.0f - 1.0f) * 12.0f, -12.0f, 12.0f);

    snapshot.clouds_position = clouds_position;
    snapshot.clouds_size = clouds_size;
    snapshot.clouds_density = clouds_density;
    snapshot.clouds_texture = clouds_texture;
    snapshot.clouds_feedback = clouds_feedback;
    snapshot.clouds_reverb = clouds_reverb;
    snapshot.clouds_dry_wet = clouds_dry_wet;
    snapshot.clouds_pitch = clouds_pitch;
    snapshot.pitch = g_hardware.GetPitchKnob().Value();                         // ADC 7

    g_controls.UpdateControlSnapshot(snapshot);
}
