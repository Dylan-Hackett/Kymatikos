#include "mpr121.h"
#include "daisy.h"
#include "hid/logger.h"

bool kymatikos_hal::Mpr121::Init(const kymatikos_hal::Mpr121::Config& config)
{
    i2c_address_ = config.i2c_address;

    i2c_handle_.Init(config.i2c_config);
    transport_error_ = false; 
    WriteRegister(MPR121_SOFTRESET, 0x63);
    daisy::System::Delay(1);

    auto cfg2 = ReadRegister8(MPR121_CONFIG2);
    if(cfg2 != 0x24)
    {
        daisy::Logger<>::PrintLine("MPR121: CONFIG2 mismatch (0x%02X)", cfg2);
        SetTransportErr(true);
        return false;
    }

    WriteRegister(MPR121_ECR, 0x00);

    SetThresholds(8, 4);

    WriteRegister(MPR121_MHDR, 0x01);
    WriteRegister(MPR121_NHDR, 0x03);
    WriteRegister(MPR121_NCLR, 0x10);
    WriteRegister(MPR121_FDLR, 0x20);

    WriteRegister(MPR121_MHDF, 0x01);
    WriteRegister(MPR121_NHDF, 0x03);
    WriteRegister(MPR121_NCLF, 0x10);
    WriteRegister(MPR121_FDLF, 0x20);

    WriteRegister(MPR121_NHDT, 0x01);
    WriteRegister(MPR121_NCLT, 0x05);
    WriteRegister(MPR121_FDLT, 0x00);

    WriteRegister(MPR121_DEBOUNCE, (2 << 4) | 2);
    WriteRegister(MPR121_CONFIG1, 0x3F);
    WriteRegister(MPR121_CONFIG2, 0x00);

    WriteRegister(MPR121_ECR, 0x8F);

    return !transport_error_;
}

uint16_t kymatikos_hal::Mpr121::Touched(void)
{
    return ReadRegister16(MPR121_TOUCHSTATUS_L);
}

uint16_t kymatikos_hal::Mpr121::FilteredData(uint8_t channel)
{
    if(channel > 12)
        return 0;
    return ReadRegister16(MPR121_FILTDATA_0L + channel * 2);
}

uint8_t kymatikos_hal::Mpr121::BaselineData(uint8_t channel)
{
    if(channel > 12)
        return 0;
    uint8_t bl = ReadRegister8(MPR121_BASELINE_0 + channel);
    return bl;
}

int16_t kymatikos_hal::Mpr121::GetBaselineDeviation(uint8_t channel)
{
    if (channel > 11) return 0;
    uint16_t filtered  = FilteredData(channel);
    uint8_t  baseline  = BaselineData(channel);
    return (static_cast<int16_t>(baseline) << 2) - static_cast<int16_t>(filtered);
}

float kymatikos_hal::Mpr121::GetProximityValue(const uint16_t channelMask, float sensitivity)
{
    if (sensitivity <= 0.0f) sensitivity = 1.0f;
    int16_t total_deviation = 0;
    int active_channels = 0;

    for (uint8_t i = 0; i < 12; ++i) {
        if ((channelMask >> i) & 0x01) {
            uint16_t filtered_data  = FilteredData(i);
            uint8_t  baseline_data  = BaselineData(i);
            int16_t deviation = (static_cast<int16_t>(baseline_data) << 2) - static_cast<int16_t>(filtered_data);
            
            if (deviation > 0) {
                total_deviation += deviation;
            }
            active_channels++;
        }
    }

    if (active_channels == 0) return 0.0f;

    float average_deviation = static_cast<float>(total_deviation) / active_channels;
    float proximity_value = (average_deviation / (1023.0f * sensitivity));

    return fmaxf(0.0f, fminf(1.0f, proximity_value));
}

void kymatikos_hal::Mpr121::SetThresholds(uint8_t touch, uint8_t release)
{
    WriteRegister(MPR121_ECR, 0x00);
    for(uint8_t i = 0; i < 12; i++)
    {
        WriteRegister(MPR121_TOUCHTH_0 + 2 * i, touch);
        WriteRegister(MPR121_RELEASETH_0 + 2 * i, release);
    }
    WriteRegister(MPR121_ECR, 0x8F);
}

uint8_t kymatikos_hal::Mpr121::ReadRegister8(uint8_t reg)
{
    uint8_t value = 0;
    auto res = i2c_handle_.ReadDataAtAddress(i2c_address_, reg, 1, &value, 1, kTimeout);
    SetTransportErr(res != daisy::I2CHandle::Result::OK);
    return value;
}

uint16_t kymatikos_hal::Mpr121::ReadRegister16(uint8_t reg)
{
    uint8_t buffer[2];
    auto res = i2c_handle_.ReadDataAtAddress(i2c_address_, reg, 1, buffer, 2, kTimeout);
    SetTransportErr(res != daisy::I2CHandle::Result::OK);
    if (res == daisy::I2CHandle::Result::OK) {
        return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
    }
    return 0;
}

void kymatikos_hal::Mpr121::WriteRegister(uint8_t reg, uint8_t value)
{
    uint8_t buffer[1] = {value};
    auto res = i2c_handle_.WriteDataAtAddress(i2c_address_, reg, 1, buffer, 1, kTimeout);
    SetTransportErr(res != daisy::I2CHandle::Result::OK);
}

bool kymatikos_hal::Mpr121::HasError() const { return transport_error_; }

void kymatikos_hal::Mpr121::ClearError() { transport_error_ = false; }
