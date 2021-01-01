#pragma once

#include <vector>
#include <chrono>
#include <array>

#include "Atom.h"
#include "Device.h"

namespace ps
{
  namespace c = std::chrono;

  struct PadState
  {
    atom::PadMode Mode;
    atom::Color Color;

    /// Append notes needed to change to given mode and color.
    void toNotes(atom::Pad p, std::vector<atom::Note>&) const;
  };

  struct AnimStep
  {
    atom::Pad Pad;
    PadState State;
    c::milliseconds Time;
  };

  using Animation = std::vector<AnimStep>;

  /// Remember the states of the PADs, play animations on top of it.
  class Pads
  {
  public:
    Pads(Device& device);

    void setPad(atom::Pad, atom::PadMode, atom::Color);

    void startPlaying(Animation, bool repeat);

    void poll();

  private:
    void step(); // TODO: is this the same as poll exactly ?
    void reset();

    Device& _device;
    std::array<PadState, 16> _pads;

    Animation _animation;
    bool _repeat = false;
    unsigned _stepIndex = 0;
    c::system_clock::time_point _start = {};
  };

  Animation caterpillar();
}
