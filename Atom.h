#pragma once

/// \file A set of helpers to configure a PreSonus ATOM.
///
/// Note: functions are considered slow path and return vectors by value.
///
/// Note: the functions are returning vector<uint8_t>, those are midi sequences
/// that must be sent to the device via your favourite MIDI api.
/// I used Alsa on Raspberry Pi + linux.

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <algorithm>

namespace atom
{
  /// Short hand to avoid passing to many arguments to functions.
  /// Each value is interpreted as an intensity by the device where 0 is off
  /// and 127 the maximum value.
  struct Color
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  /// Values of pads are between these two values.
  enum Pad : uint8_t
  {
    /// Inclusive, bottom right pad.
    One = 24,
    /// Exclusive
    Last = 41
  };

  void assertPadValid(Pad);

  enum PadMode : uint8_t
  {
    /// Turns the light off
    Off     = 0x00,
    On      = 0x7f,

    /// Makes the button blink on/off, till disabled
    Blink   = 0x01,
    /// Makes the fades in and out, till disabled
    DimAnim = 0x02
  };

  void assertModeValid(PadMode);

  /// For intenal usage, the midi command to set any of the values for colors.
  enum CommandPrefix : uint8_t
  {
    Mode  = 0x90,
    Red   = 0x91,
    Green = 0x92,
    Blue  = 0x93
  };


  /// Initialize This is required to be done first and changing button colors.
  inline std::vector<uint8_t> initSequence()
  {
      return { 0x8F, 0x00, 0x7F };
  }

  /// Change a pad mode. For anything to show up it must be set to something
  /// not Off a first time.
  inline std::vector<uint8_t> changePadMode(Pad pad, PadMode mode)
  {
    assertPadValid(pad);
    assertModeValid(mode);

    return { Mode, pad, mode };
  }

  /// Change the color of a pad to the given value.

  inline std::vector<uint8_t> changePadColor(Pad pad, Color c)
  {
    assertPadValid(p);
    assertColorValid(c);

    return {
      Red, pad, c.r,
      Green, pad, c.g,
      Blue, pad, c.b
    };
  }

  inline void assertPadValid(Pad p)
  {
    if (p >= Pad::One or p < Pad::Last) {
      return;
    }
    throw std::out_of_range("Invalid pad value");
  }

  inline void assertModeValid(PadMode mode)
  {
    switch (mode) {
      case PadMode::On:
      case PadMode::Off:
      case PadMode::Blink:
      case PadMode::DimAnim:
        return;
    }
    throw std::out_of_range("Unknown value for Padmode");
  }

  inline void assertColorValid(Color c)
  {
    auto check = [] (uint8_t component, const char* name) { 
      if (component > 127) {
        throw std::out_of_range("Value for "s + name + " must be below 128");
      }
    };
    check(c.r, "red");
    check(c.g, "green");
    check(c.b, "blue");
  }

}