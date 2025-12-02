#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "Nimbus_SM/dsp/granular_processor.h"
#include "daisy_patch_sm.h"

/**
 * AudioEngine encapsulates audio processing components:
 * - Clouds GranularProcessor (granular effects)
 * - Audio buffers for Clouds processing
 *
 * Simplified from previous polyphonic architecture to focus on
 * keyboard-controlled granular processing.
 */
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine() = default;

    // Initialize the audio engine
    void Init(daisy::patch_sm::DaisyPatchSM* hw);

    // Get Clouds processor
    GranularProcessorClouds& GetCloudsProcessor() { return clouds_processor_; }

    // Get Clouds buffers
    uint8_t* GetCloudBuffer() { return cloud_buffer_; }
    // Restore Nimbus/Clouds original buffer sizes
    static constexpr size_t CLOUD_BUFFER_SIZE = 356352;  // loop delay storage

    uint8_t* GetCloudBufferCCM() { return cloud_buffer_ccm_; }
    static constexpr size_t CLOUD_BUFFER_CCM_SIZE = 196224;  // 65408 * 3

private:
    // Clouds processor
    GranularProcessorClouds clouds_processor_;

    // Pointers to SDRAM buffers (actual buffers defined in AudioEngine.cpp with DSY_SDRAM_BSS)
    uint8_t* cloud_buffer_;
    uint8_t* cloud_buffer_ccm_;
};

#endif // AUDIO_ENGINE_H
