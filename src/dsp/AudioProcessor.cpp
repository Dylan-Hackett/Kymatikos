#include "Thaumazein.h"
#include "mpr121_daisy.h"
#include "AudioConfig.h"
#include <cmath>
#include <algorithm>

// --- Namespace imports (local to this implementation file) ---
using namespace daisy;
using namespace daisysp;
using namespace stmlib;
using namespace infrasonic;

// Helper function declarations
void UpdateCloudsParameters(const ControlsManager::ControlSnapshot& controls_snapshot,
                            float touch_cv);
void ProcessAudioThroughClouds(AudioHandle::InterleavingInputBuffer in,
                               AudioHandle::InterleavingOutputBuffer out,
                               size_t size);
void UpdatePerformanceMonitors(size_t size, AudioHandle::InterleavingOutputBuffer out);
void UpdateArpeggiator();

// Touch state tracking for keyboard logic
static uint16_t last_touch_state = 0;

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                 AudioHandle::InterleavingOutputBuffer out,
                 size_t size) {
    // Audio ISR - keep minimal and deterministic
    g_hardware.GetCpuMeter().OnBlockStart();

    // Sync control snapshot from UI thread
    g_controls.SyncAudioControlSnapshot();
    const auto& controls_snapshot = g_controls.GetAudioControlSnapshot();
    const float touch_cv = g_controls.GetTouchCVValue();

    // Update arpeggiator state (keyboard logic preserved)
    UpdateArpeggiator();

    // Update Clouds parameters based on knobs and keyboard/touch input
    UpdateCloudsParameters(controls_snapshot, touch_cv);

    // Process audio input through Clouds and output
    ProcessAudioThroughClouds(in, out, size);

    // Clouds Integration: Call Prepare() for grain buffer preparation
    g_audio_engine.GetCloudsProcessor().Prepare();

    g_hardware.GetCpuMeter().OnBlockEnd();
}

void UpdateArpeggiator() {
    // Clear arpeggiator notes if requested
    if(g_controls.ConsumeArpClearRequest()) {
        g_controls.GetArpeggiator().ClearNotes();
    }

    bool current_arp_on = g_controls.IsArpEnabled();

    // Update arpeggiator with current touch state (keyboard logic preserved)
    if (current_arp_on) {
        uint16_t current_touch_state = g_controls.GetCurrentTouchState();
        g_controls.GetArpeggiator().UpdateHeldNotes(current_touch_state, last_touch_state);
        g_controls.GetArpeggiator().Process(BLOCK_SIZE);
        last_touch_state = current_touch_state;
    } else {
        // When arp is disabled, still track touch state changes
        last_touch_state = g_controls.GetCurrentTouchState();
    }

    g_controls.WasArpOn() = current_arp_on;
}

void UpdateCloudsParameters(const ControlsManager::ControlSnapshot& controls_snapshot,
                            float touch_cv) {
    clouds::Parameters* parameters = g_audio_engine.GetCloudsProcessor().mutable_parameters();
    if(!parameters) {
        return;
    }

    // Modulate morph with touch pressure (keyboard interaction preserved)
    float modulated_morph = 0.25f * controls_snapshot.morph_knob + 0.75f * touch_cv;
    modulated_morph = fclamp(modulated_morph, 0.0f, 1.0f);

    // Map knobs and touch to Clouds parameters
    // These mappings can be customized based on desired keyboard control behavior
    parameters->pitch = modulated_morph;
    parameters->texture = controls_snapshot.pitch;
    parameters->density = controls_snapshot.delay_time;
    parameters->position = touch_cv;  // Touch pressure controls grain position
    parameters->size = controls_snapshot.harm_knob;
    parameters->dry_wet = controls_snapshot.delay_mix_feedback;
    parameters->feedback = 0.1f;
    parameters->reverb = controls_snapshot.delay_mix_feedback;
    parameters->stereo_spread = controls_snapshot.delay_mix_feedback;
    parameters->freeze = (controls_snapshot.mod_wheel > 0.3f);
}

void ProcessAudioThroughClouds(AudioHandle::InterleavingInputBuffer in,
                               AudioHandle::InterleavingOutputBuffer out,
                               size_t size) {
    static clouds::ShortFrame input_frames[BLOCK_SIZE];
    static clouds::ShortFrame output_frames[BLOCK_SIZE];

    // Convert input audio to Clouds format (16-bit signed stereo frames)
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        // Read from stereo input and convert to int16 range
        // Note: 'in' is an interleaved stereo buffer, so in[i*2] = L, in[i*2+1] = R
        float inL = (i*2 < size) ? in[i*2] : 0.0f;
        float inR = (i*2+1 < size) ? in[i*2+1] : 0.0f;

        // Convert to int16 range (-32768 to 32767)
        float scaledL = inL * 32768.0f;
        float scaledR = inR * 32768.0f;

        // Clamp to int16 range
        if (scaledL > 32767.f) scaledL = 32767.f;
        if (scaledL < -32768.f) scaledL = -32768.f;
        if (scaledR > 32767.f) scaledR = 32767.f;
        if (scaledR < -32768.f) scaledR = -32768.f;

        input_frames[i].l = static_cast<int16_t>(scaledL);
        input_frames[i].r = static_cast<int16_t>(scaledR);
    }

    // Process through Clouds granular engine
    g_audio_engine.GetCloudsProcessor().Process(input_frames, output_frames, BLOCK_SIZE);

    // Convert Clouds output to float and write to output buffer
    for (size_t i = 0; i < size; i += 2) {
        // Convert from int16 to float range [-1.0, 1.0]
        float outL = static_cast<float>(output_frames[i/2].l) / 32768.0f;
        float outR = static_cast<float>(output_frames[i/2].r) / 32768.0f;

        // Apply master volume
        outL *= MASTER_VOLUME;
        outR *= MASTER_VOLUME;

        // Write stereo output
        out[i]   = outL;
        out[i+1] = outR;
    }

    UpdatePerformanceMonitors(size, out);
}

void UpdatePerformanceMonitors(size_t size, AudioHandle::InterleavingOutputBuffer out) {
    if (size > 0) {
        float current_level = fabsf(out[0]);
        g_controls.GetSmoothedOutputLevel() = g_controls.GetSmoothedOutputLevel() * 0.99f + current_level * 0.01f;
    }

    static uint32_t display_counter = 0;
    static const uint32_t display_interval_blocks = (uint32_t)(g_hardware.GetSampleRate() / BLOCK_SIZE * 3.0f);
    if (++display_counter >= display_interval_blocks) {
        display_counter = 0;
        g_controls.ShouldUpdateDisplay() = true;
    }
}
