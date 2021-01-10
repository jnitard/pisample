#pragma once

/// \file Some helpers for ALSA.

#include <alsa/asoundlib.h>

#include "fmt.h"

#include <memory>

namespace ps
{
  struct AlsaErr
  {
    long Err;
  };
  
  template <class OStream>
  OStream& operator<<(OStream& out, AlsaErr err)
  {
    out << snd_strerror(err.Err);
    return out;
  }

  /// Turn an event into string, mostly useful for debugging.
  const char* eventToString(snd_seq_event_type_t event);

  struct FrameFormat
  {
    int Rate;
    int Bits;
    int Channels; // when not given explicitely, card may have 10 channels
                  // we need to read or write but only care about 2
  };

  /// Wraps snd_pcm_t construction and destruction. Determine a sample format
  /// that can be used with that card, start with the higher ones for best
  /// quality first and then goes down. I am disabling that for now.
  /// TODO: make sample format configurable ?
  struct Pcm
  {
    // Try some easy to implement format first.
    // If a format is not supported I’d recommend switching input to pulse
    // at much more CPU expense.
    Pcm(std::string_view interface,
        snd_pcm_stream_t direction,
        int expectedChannelCount,
        std::array<int, 2> channels);
    Pcm(const Pcm&) = delete;
    ~Pcm();

    // NOTE: can’t be held by value and is used via pointers by most C-apis,
    // so keeping a raw pointer.
    snd_pcm_t* Ptr;
    FrameFormat Format;
  };

  inline int formatBits(snd_pcm_format_t format)
  {
    switch (format) {
      case SND_PCM_FORMAT_S16_LE: return 16;
      case SND_PCM_FORMAT_S24_LE: return 24;
      case SND_PCM_FORMAT_S32_LE: return 32;
      default:
        throw Exception("Unsupported format {}", (int)format);
    }
    __builtin_unreachable();
  }

  inline snd_pcm_format_t bitsToFormat(int bits)
  {
    switch (bits) {
      case 16: return SND_PCM_FORMAT_S16_LE;
      case 24: return SND_PCM_FORMAT_S24_LE;
      case 32: return SND_PCM_FORMAT_S32_LE;
      default:
        throw Exception("Unsupported number of bits per sample: {}", bits);
    }
    __builtin_unreachable();
  }
}
