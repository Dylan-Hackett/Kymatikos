#include "Kymatikos.h"
#include "mpr121_daisy.h"
#include "AudioConfig.h"
#include <cmath>
#include <algorithm>

// --- Namespace imports (local to this implementation file) ---
using namespace daisy;
using namespace daisysp;

namespace
{
constexpr float kInputGain = 2.0f;
constexpr float kOutputGain =2.0f;

FloatFrame g_clouds_in[BLOCK_SIZE];
FloatFrame g_clouds_out[BLOCK_SIZE];

void UpdateCloudsParameters(GranularProcessorClouds& processor)
{
    const auto& controls = g_controls.GetAudioControlSnapshot();
    Parameters* params = processor.mutable_parameters();

    params->position      = controls.clouds_position;
    params->density       = controls.clouds_density;
    params->size          = controls.clouds_size;
    params->texture       = controls.clouds_texture;
    params->stereo_spread = 0.5f;
    params->feedback      = controls.clouds_feedback;
    params->reverb        = controls.clouds_reverb;
    params->pitch         = controls.clouds_pitch;
    params->dry_wet       = controls.clouds_dry_wet;
    params->freeze        = false;
    params->trigger       = false;
}
} // namespace

// Helper function declarations
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

    // Prepare Clouds state in the audio thread to avoid races with main loop
    g_audio_engine.GetCloudsProcessor().Prepare();

    // Update arpeggiator state (keyboard logic preserved)
    UpdateArpeggiator();

    // Process audio input through simplified DSP path and output
    ProcessAudioThroughClouds(in, out, size);

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

    g_controls.SetWasArpOn(current_arp_on);
}

void ProcessAudioThroughClouds(AudioHandle::InterleavingInputBuffer in,
                               AudioHandle::InterleavingOutputBuffer out,
                               size_t size) {
    auto& processor = g_audio_engine.GetCloudsProcessor();
    UpdateCloudsParameters(processor);

    float block_peak = 0.0f;

    const size_t total_frames = size / 2;
    const size_t frame_count  = std::min(total_frames, static_cast<size_t>(BLOCK_SIZE));

    for(size_t frame = 0; frame < frame_count; ++frame) {
        const size_t idx = frame * 2;
        float input_l = in ? in[idx] : 0.0f;
        float input_r = in ? in[idx + 1] : input_l;
        input_l *= kInputGain;
        input_r *= kInputGain;
        block_peak = fmaxf(block_peak, fmaxf(fabsf(input_l), fabsf(input_r)));

        g_clouds_in[frame].l = daisysp::fclamp(input_l, -1.0f, 1.0f);
        g_clouds_in[frame].r = daisysp::fclamp(input_r, -1.0f, 1.0f);
    }

    processor.Process(g_clouds_in, g_clouds_out, frame_count);

    g_controls.SetInputPeakLevel(block_peak);

    const float master_vol = g_controls.GetAudioControlSnapshot().master_volume;
    for(size_t frame = 0; frame < frame_count; ++frame) {
        const size_t idx = frame * 2;
        // Mono output to left channel only (use left channel, no summing to avoid combing)
        float mono = g_clouds_out[frame].l;
        out[idx]     = mono * kOutputGain * master_vol;
        out[idx + 1] = 0.0f;
    }

    for(size_t frame = frame_count; frame < total_frames; ++frame) {
        const size_t idx = frame * 2;
        out[idx]     = 0.0f;
        out[idx + 1] = 0.0f;
    }

    UpdatePerformanceMonitors(size, out);
}

void UpdatePerformanceMonitors(size_t size, AudioHandle::InterleavingOutputBuffer out) {
    if (size > 0) {
        float current_level = fabsf(out[0]);
        float prev = g_controls.GetSmoothedOutputLevel();
        g_controls.SetSmoothedOutputLevel(prev * 0.99f + current_level * 0.01f);
    }

    static uint32_t display_counter = 0;
    static const uint32_t display_interval_blocks = (uint32_t)(g_hardware.GetSampleRate() / BLOCK_SIZE * 3.0f);
    if (++display_counter >= display_interval_blocks) {
        display_counter = 0;
        g_controls.SetUpdateDisplay(true);
    }
}
