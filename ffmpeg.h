#pragma once

#include <filesystem>
#include <vector>

#include "Log.h"

namespace ps
{
  enum class Endianness_t
  {
    Big,
    Little
  };

  enum class Interleaving_t
  {
    /// Samples are grouped by time frame first, one frame containing all channels.
    Yes,
    /// Samples are grouped per channels first, each channels containing samples
    /// in the same order.
    No,
  };

  enum class BitDepth_t
  {
    // 2 bytes per samples, fully used.
    BD16,
    // 4 bytes per samples, only 3 bytes used.
    BD24,
    // 4 bytes per samples.
    BD32,
  };

  enum class Representation_t
  {
    Signed,
    Unsigned,
    Float
  };

  struct SampleFormat
  {
    Interleaving_t Interleaving;
    int SampleRate;
    Representation_t Repr;
    int BitDepth;
    Endianness_t Endianness;
  };

  struct AudioData
  {
    SampleFormat Format;
    std::vector<uint8_t> Samples;
  };

  /// ALWAYS expecting two channels for now. Thinking to duplicate the mono
  /// channels here.
  /// This is obviously not 
  AudioData readAudioFile(const std::filesystem::path&);

  /// Convert samples to a new format.
  AudioData convert(AudioData&& input, SampleFormat newFormat);

}