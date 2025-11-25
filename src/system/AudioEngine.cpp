#include "AudioEngine.h"
#include "Nimbus_SM/resources.h"
#include <cstring>

// Global SDRAM buffers for Clouds (must be at file scope for DSY_SDRAM_BSS attribute)
DSY_SDRAM_BSS static uint8_t g_cloud_buffer[AudioEngine::CLOUD_BUFFER_SIZE];
DSY_SDRAM_BSS static uint8_t g_cloud_buffer_ccm[AudioEngine::CLOUD_BUFFER_CCM_SIZE];

AudioEngine::AudioEngine()
    : cloud_buffer_(g_cloud_buffer),
      cloud_buffer_ccm_(g_cloud_buffer_ccm) {
}

void AudioEngine::Init(daisy::patch_sm::DaisyPatchSM* hw) {
    const float sample_rate = hw ? hw->AudioSampleRate() : 48000.0f;

    memset(cloud_buffer_, 0, AudioEngine::CLOUD_BUFFER_SIZE);
    memset(cloud_buffer_ccm_, 0, AudioEngine::CLOUD_BUFFER_CCM_SIZE);

    InitResources(sample_rate);
    clouds_processor_.Init(sample_rate,
                           cloud_buffer_,
                           AudioEngine::CLOUD_BUFFER_SIZE,
                           cloud_buffer_ccm_,
                           AudioEngine::CLOUD_BUFFER_CCM_SIZE);

    clouds_processor_.mutable_parameters()->dry_wet = 0.0f;
    clouds_processor_.mutable_parameters()->freeze = false;
    clouds_processor_.set_playback_mode(PLAYBACK_MODE_LOOPING_DELAY);
}
