#include "daisy_patch_sm.h"
#include "daisysp.h"

#include "clouds/dsp/granular_processor.h"
#include "clouds/dsp/parameters.h"

using namespace daisy;
using namespace daisy::patch_sm;
using namespace daisysp;

namespace {

constexpr size_t kBlockSize = 32;

DaisyPatchSM hw;
clouds::GranularProcessor processor;

DSY_SDRAM_BSS uint8_t block_mem[118784 * 2];
DSY_SDRAM_BSS uint8_t block_ccm[(65536 * 2) - 128];

clouds::ShortFrame input_block[kBlockSize];
clouds::ShortFrame output_block[kBlockSize];

inline int16_t FloatToShort(float sample) {
    sample = fclamp(sample, -1.0f, 1.0f);
    return static_cast<int16_t>(sample * 32767.0f);
}

inline float ShortToFloat(int16_t sample) {
    return static_cast<float>(sample) / 32768.0f;
}

void InitProcessor() {
    processor.Init(block_mem, sizeof(block_mem), block_ccm, sizeof(block_ccm));
    processor.set_quality(0); // 16-bit stereo by default
    processor.set_playback_mode(clouds::PLAYBACK_MODE_GRANULAR);

    clouds::Parameters* params = processor.mutable_parameters();
    params->position = 0.5f;
    params->size = 0.5f;
    params->pitch = 0.0f;
    params->density = 0.5f;
    params->texture = 0.5f;
    params->dry_wet = 1.0f;
    params->feedback = 0.2f;
    params->reverb = 0.2f;
    params->stereo_spread = 0.5f;
    params->freeze = false;
    params->trigger = false;
}

void UpdateParameters() {
    hw.ProcessAllControls();
    clouds::Parameters* params = processor.mutable_parameters();

    params->position = fclamp(hw.GetAdcValue(CV_1), 0.0f, 1.0f);
    params->size = fclamp(hw.GetAdcValue(CV_2), 0.05f, 1.0f);
    params->density = fclamp(hw.GetAdcValue(CV_3), 0.0f, 1.0f);
    params->texture = fclamp(hw.GetAdcValue(CV_4), 0.0f, 1.0f);

    float pitch_cv = hw.GetAdcValue(CV_5);
    params->pitch = (pitch_cv - 0.5f) * 48.0f; // +/- 2 octaves

    params->stereo_spread = fclamp(hw.GetAdcValue(CV_6), 0.0f, 1.0f);
    params->dry_wet = fclamp(hw.GetAdcValue(CV_7), 0.0f, 1.0f);
    params->reverb = fclamp(hw.GetAdcValue(CV_8), 0.0f, 1.0f);

    params->feedback = 0.25f + 0.5f * params->dry_wet;
    params->freeze = hw.gate_in_1.State();
    params->trigger = hw.gate_in_2.State();
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
    UpdateParameters();

    for(size_t i = 0; i < size; ++i) {
        input_block[i].l = FloatToShort(in[0][i]);
        input_block[i].r = FloatToShort(in[1][i]);
    }

    processor.Process(input_block, output_block, size);

    for(size_t i = 0; i < size; ++i) {
        out[0][i] = ShortToFloat(output_block[i].l);
        out[1][i] = ShortToFloat(output_block[i].r);
    }
}

} // namespace

int main(void) {
    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ);
    hw.SetAudioBlockSize(kBlockSize);

    InitProcessor();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(1) {
        processor.Prepare();
        hw.Delay(1);
    }
}
