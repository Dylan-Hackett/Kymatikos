#include "Kymatikos.h"
#include "mpr121.h"
#include "settings.h"
#include "Nimbus_SM/resources.h"
#include <cmath>
#include <algorithm>
#include <cstring>

using namespace daisy;
using namespace daisysp;

namespace {
constexpr float kInputGain = 1.0f;
constexpr float kOutputGain = 1.0f;
constexpr size_t CLOUD_BUF_SIZE = 356352;
constexpr size_t CLOUD_CCM_SIZE = 196224;

DSY_SDRAM_BSS static uint8_t g_cloud_buf[CLOUD_BUF_SIZE];
DSY_SDRAM_BSS static uint8_t g_cloud_ccm[CLOUD_CCM_SIZE];
static GranularProcessorClouds g_clouds;

FloatFrame g_clouds_in[BLOCK_SIZE];
FloatFrame g_clouds_out[BLOCK_SIZE];

void UpdateCloudsParams() {
    const auto& c = g_controls.GetAudioControlSnapshot();
    Parameters* p = g_clouds.mutable_parameters();
    p->position = c.clouds_position;
    p->density = c.clouds_density;
    p->size = c.clouds_size;
    p->texture = c.clouds_texture;
    p->stereo_spread = 0.5f;
    p->feedback = c.clouds_feedback;
    p->reverb = c.clouds_reverb;
    p->pitch = c.clouds_pitch;
    p->dry_wet = c.clouds_dry_wet;
    p->freeze = false;
    p->trigger = false;
}
}

static uint16_t last_touch_state = 0;

void InitClouds(float sample_rate) {
    memset(g_cloud_buf, 0, CLOUD_BUF_SIZE);
    memset(g_cloud_ccm, 0, CLOUD_CCM_SIZE);
    InitResources(sample_rate);
    g_clouds.Init(sample_rate, g_cloud_buf, CLOUD_BUF_SIZE, g_cloud_ccm, CLOUD_CCM_SIZE);
    g_clouds.mutable_parameters()->dry_wet = 0.0f;
    g_clouds.mutable_parameters()->freeze = false;
    g_clouds.set_playback_mode(PLAYBACK_MODE_LOOPING_DELAY);
}

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t size) {
    g_hardware.GetCpuMeter().OnBlockStart();
    g_controls.SyncAudioControlSnapshot();
    g_clouds.Prepare();

    if(g_controls.ConsumeArpClearRequest()) {
        g_controls.GetArpeggiator().ClearNotes();
    }
    bool arp_on = g_controls.IsArpEnabled();
    if (arp_on) {
        uint16_t ts = g_controls.GetCurrentTouchState();
        g_controls.GetArpeggiator().UpdateHeldNotes(ts, last_touch_state);
        g_controls.GetArpeggiator().Process(BLOCK_SIZE);
        last_touch_state = ts;
    } else {
        last_touch_state = g_controls.GetCurrentTouchState();
    }
    g_controls.SetWasArpOn(arp_on);

    UpdateCloudsParams();
    float block_peak = 0.0f;
    const size_t total_frames = size / 2;
    const size_t frame_count = std::min(total_frames, static_cast<size_t>(BLOCK_SIZE));

    for(size_t f = 0; f < frame_count; ++f) {
        float input = in ? in[f * 2] : 0.0f;
        input *= kInputGain;
        block_peak = fmaxf(block_peak, fabsf(input));
        float clamped = daisysp::fclamp(input, -1.0f, 1.0f);
        g_clouds_in[f].l = clamped;
        g_clouds_in[f].r = clamped;
    }

    g_clouds.Process(g_clouds_in, g_clouds_out, frame_count);
    g_controls.SetInputPeakLevel(block_peak);

    const float vol = g_controls.GetAudioControlSnapshot().master_volume;
    for(size_t f = 0; f < frame_count; ++f) {
        out[f * 2] = g_clouds_out[f].l * kOutputGain * vol;
        out[f * 2 + 1] = 0.0f;
    }
    for(size_t f = frame_count; f < total_frames; ++f) {
        out[f * 2] = 0.0f;
        out[f * 2 + 1] = 0.0f;
    }

    if (size > 0) {
        float prev = g_controls.GetSmoothedOutputLevel();
        g_controls.SetSmoothedOutputLevel(prev * 0.99f + fabsf(out[0]) * 0.01f);
    }
    static uint32_t disp_cnt = 0;
    static const uint32_t disp_interval = (uint32_t)(g_hardware.GetSampleRate() / BLOCK_SIZE * 3.0f);
    if (++disp_cnt >= disp_interval) {
        disp_cnt = 0;
        g_controls.SetUpdateDisplay(true);
    }

    g_hardware.GetCpuMeter().OnBlockEnd();
}
