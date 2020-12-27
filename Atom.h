#pragma once

/// \file A set of helpers to configure a PreSonus ATOM.
///
/// Note: functions are considered slow path and return vectors by value.
///
/// Note: the functions are returning vector<Note>, those are midi sequences
/// that must be sent to the device via your favourite MIDI api.
/// I used Alsa on Raspberry Pi + linux.

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <algorithm>

namespace atom
{
  /// Output of most functions.
  struct Note
  {
    bool OnOff;
    uint8_t Channel;
    uint8_t Note;
    uint8_t Velocity;
  };

  constexpr int NumPads = 16;

  /// Short hand to avoid passing to many arguments to functions.
  /// Each value is interpreted as an intensity by the device where 0 is off
  /// and 127 the maximum value.
  struct Color
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  void assertColorValid(Color c);

  /// Values of pads are between these two values.
  enum class Pad : uint8_t
  {
    None = 0,
    /// Inclusive, bottom right pad.
    One = 0x24,
    /// Exclusive
    Last = 0x24 + 16
  };
  constexpr uint8_t operator-(Pad lhs, Pad rhs)
  {
    return uint8_t(lhs) - uint8_t(rhs);
  }

  template <class Func>
  void forAllPads(Func&& f)
  {
    for (uint8_t i = (uint8_t)atom::Pad::One; i < (uint8_t)atom::Pad::Last; ++i) {
      std::forward<Func>(f)(atom::Pad(i));
    }
  }

  void assertPadValid(Pad);

  enum class PadMode : uint8_t
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
  enum class CommandPrefix : uint8_t
  {
    Mode  = 0x0,
    Red   = 0x1,
    Green = 0x2,
    Blue  = 0x3
  };


  /// Initialize This is required to be done first and changing button colors.
  inline std::vector<Note> initSequence()
  {
      return { Note{ 
        .OnOff = false,
        .Channel = 0,
        .Note = 0,
        .Velocity = 0x7F
      } };
  }

  /// Change a pad mode. For anything to show up it must be set to something
  /// not Off a first time.
  inline std::vector<Note> changePadMode(Pad pad, PadMode mode)
  {
    assertPadValid(pad);
    assertModeValid(mode);

    return { Note{
      .OnOff = true,
      .Channel = static_cast<uint8_t>(CommandPrefix::Mode),
      .Note = static_cast<uint8_t>(pad),
      .Velocity = 0x7f
    } };
  }

  /// Change the color of a pad to the given value.
  inline std::vector<Note> changePadColor(Pad pad, Color c)
  {
    assertPadValid(pad);
    assertColorValid(c);

    return {
      Note{ true, (uint8_t)CommandPrefix::Red,   (uint8_t)pad, c.r },
      Note{ true, (uint8_t)CommandPrefix::Green, (uint8_t)pad, c.g },
      Note{ true, (uint8_t)CommandPrefix::Blue,  (uint8_t)pad, c.b }
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
    using namespace std::string_literals;
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