#pragma once

/// \file A set of helpers to configure a PreSonus ATOM.
///
/// Note: functions are considered slow path and return vectors by value.
///
/// Note: the functions are returning vector<Note>, those are midi sequences
/// that must be sent to the device via your favourite MIDI api.
/// I used Alsa on Raspberry Pi + linux.
///
/// NOTE: should only depend on the standard library

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <string_view>

namespace atom
{
  /// What you need to talk to a MIDI API and send pad commands.
  struct Note
  {
    bool OnOff;
    uint8_t Channel;
    uint8_t Note;
    uint8_t Velocity;
  };

  constexpr int NumPads = 16;

  /// MIDI header = 0xB0 (i.e. channel is always 0)
  struct Control
  {
    uint8_t Param;
    uint8_t Value;
  };

  /// Short hand to avoid passing to many arguments to functions.
  /// Each value is interpreted as an intensity by the device where 0 is off
  /// and 127 the maximum value.
  struct Color
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    /// Create a color from an HTML hex representation i.e. #123456
    static Color fromString(std::string_view);
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
  constexpr Pad operator+(Pad lhs, int rhs)
  {
    return static_cast<Pad>(uint8_t(lhs) + rhs);
  }
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
    /// Turns the light on or off.
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

  /// type 'controller', channel 0, values 0 or 127.
  /// These can be set on or off via the same value they send us.
  enum class Buttons : uint8_t
  {
    // Left column
    Nudge = 30,
    Setup = 86,
    SetLoop = 85,
    Editor = 31,
    ShowHide = 29,
    Preset = 27,
    Bank = 26,
    FullLevel = 25,
    NoteRepeat = 24,
    Shift = 32,

    // Right column
    Up = 87,
    Down = 89,
    Left = 90,
    Right = 102,
    Select = 103,
    Zoom = 104,
    Click = 105,
    Record = 107,
    Play = 109,
    Stop = 111,
  };

  inline Control switchButton(Buttons b, bool on)
  {
    return Control{ .Param = uint8_t(b), .Value = uint8_t(on ? 0x7F : 0x00) };
  }

  template <class Func>
  void forAllButtons(Func&& f)
  {
    for (uint8_t b = 0; b < 128; ++b) {
      auto button = static_cast<Buttons>(b);
      switch (button) {
        case Buttons::Nudge:
        case Buttons::Setup:
        case Buttons::SetLoop:
        case Buttons::Editor:
        case Buttons::ShowHide:
        case Buttons::Preset:
        case Buttons::FullLevel:
        case Buttons::NoteRepeat:
        case Buttons::Shift:
        case Buttons::Up:
        case Buttons::Down:
        case Buttons::Left:
        case Buttons::Right:
        case Buttons::Select:
        case Buttons::Zoom:
        case Buttons::Click:
        case Buttons::Record:
        case Buttons::Play:
        case Buttons::Stop:
          std::forward<Func>(f)(button);
          break;
        default:
          break;
      }
    }
  }

  /// type 'controller', channel 0, values 1 for left or 65 for right
  enum class Knobs : uint8_t
  {
    One = 14,
    Two = 15,
    Three = 16,
    Four = 17,

    Left = 1,
    Right = 65
  };


  /// Initialize This is required to be done first and changing button colors.
  inline std::vector<Note> initSequence()
  {
    return { Note{
      .OnOff = false,
      .Channel = 0xf,
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
      .Velocity = static_cast<uint8_t>(mode)
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

  inline Color Color::fromString(std::string_view sv)
  {
    if (sv.empty() or sv[0] != '#') {
      throw std::runtime_error("Invalid color value " + std::string(sv));
    }

    // TODO: would need std::from_chars for OK performance.
    unsigned hex = stoul((std::string)sv.substr(1), nullptr, 16);

    return Color{
      .r = uint8_t((hex >> 16) / 2),
      .g = uint8_t(((hex >> 8) % 256) / 2),
      .b = uint8_t((hex % 256) / 2)
    };
  }
}
