#ifndef NIMBUS_STORAGE_H_
#define NIMBUS_STORAGE_H_

namespace nimbus
{
struct Settings
{
    float offset, scale;
    int   model_mode, poly_mode, egg_mode;

    bool operator==(const Settings& rhs)
    {
        return offset == rhs.offset && scale == rhs.scale
               && model_mode == rhs.model_mode && poly_mode == rhs.poly_mode
               && egg_mode == rhs.egg_mode;
    }
    bool operator!=(const Settings& rhs) { return !operator==(rhs); }
};

} // namespace nimbus

#endif // NIMBUS_STORAGE_H_