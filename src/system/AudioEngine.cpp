#include "AudioEngine.h"

// Global SDRAM buffers for Clouds (must be at file scope for DSY_SDRAM_BSS attribute)
DSY_SDRAM_BSS static uint8_t g_cloud_buffer[AudioEngine::CLOUD_BUFFER_SIZE];
DSY_SDRAM_BSS static uint8_t g_cloud_buffer_ccm[AudioEngine::CLOUD_BUFFER_CCM_SIZE];

AudioEngine::AudioEngine()
    : cloud_buffer_(g_cloud_buffer),
      cloud_buffer_ccm_(g_cloud_buffer_ccm) {
}

void AudioEngine::Init(daisy::DaisySeed* hw) {
    // Initialize Clouds granular processor
    clouds_processor_.Init(cloud_buffer_, AudioEngine::CLOUD_BUFFER_SIZE,
                           cloud_buffer_ccm_, AudioEngine::CLOUD_BUFFER_CCM_SIZE);

    // Set initial Clouds parameters
    clouds_processor_.mutable_parameters()->dry_wet = 0.0f;
    clouds_processor_.mutable_parameters()->freeze = false;

    // Force Clouds into Granular mode on startup
    clouds_processor_.set_playback_mode(clouds::PLAYBACK_MODE_GRANULAR);
}
