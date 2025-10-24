#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

#include <cstddef>

// Central definition for shared audio constants used across the firmware.
// BLOCK_SIZE matches Clouds' expected block size for processing
constexpr std::size_t BLOCK_SIZE = 32;

#endif // AUDIO_CONFIG_H
