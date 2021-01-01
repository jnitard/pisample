#include "Pads.h"
#include "Device.h"

#include <deque>
#include <cstring>
#include <algorithm>
#include <iostream>

#include <alsa/seq_event.h>

using namespace std;
using namespace atom;
using namespace ps;

void PadState::toNotes(Pad pad, vector<Note>& out) const
{
  auto c = changePadMode(pad, Mode);
  copy(begin(c), end(c), back_inserter(out));
  c = changePadColor(pad, Color);
  copy(begin(c), end(c), back_inserter(out));
}

Animation ps::caterpillar()
{
  using namespace atom;

  auto nextPad = [](Pad p) {
    if (p == Pad(uint8_t(Pad::One) + 12)) {
      return Pad::One;
    }

    uint8_t idx = p - Pad::One;
    uint8_t base = Pad::One - Pad::None;

    // Treat the last 16 pads like the first 8
    if (idx >= 8) {
      idx -= 8;
      base += 8;
    }

    if (idx < 3) {
      return static_cast<Pad>(idx + 1 + base);
    }
    if (idx == 3) {
      return static_cast<Pad>(7 + base);
    }
    if (idx == 4) {
      return static_cast<Pad>(8 + base);
    }
    // if (idx < 8) {
    return static_cast<Pad>(idx - 1 + base);
  };

  auto makeColor = [](int idx) {
    switch (idx % 4) {
    case 0: return Color{96,  0,  0};
    case 1: return Color{ 0, 96,  0};
    case 2: return Color{ 0,  0, 96};
    case 3: return Color{96, 96, 96};
    }
    __builtin_unreachable();
  };

  Animation result;
  deque<Pad> on;

  AnimStep s{
    .Pad = Pad::One,
    .State = {
      .Mode = PadMode::On,
      .Color = makeColor(0),
    },
    .Time = c::milliseconds(0),
  };

  auto frameTime = c::milliseconds(100);

  for (int i = 0; i < 4; ++i) {
    result.push_back(s);
    on.push_back(s.Pad);
    s.Time += frameTime;
    s.Pad = nextPad(on.back());
    s.State.Color = makeColor(i + 1);
  }

  forAllPads([&](...) {
    // TODO: could we do better using the midi sequencer here ?
    s.Time += frameTime;

    // Turn off tail
    s.Pad = on[0];
    s.State.Mode = PadMode::Off;
    result.push_back(s);
    on.pop_front();

    s.Pad = nextPad(on.back());
    s.State.Mode = PadMode::On;
    s.State.Color = makeColor(s.Pad - Pad::One);
    result.push_back(s);
    on.push_back(s.Pad);
  });

  return result;
}

Pads::Pads(Device& device)
  : _device(device)
{
  forAllPads([this](Pad p) {
    setPad(
      p,
      PadMode::Off,
      Color{ .r = 0, .g = 0, .b = 0 }
    );
  });

  startPlaying(caterpillar(), true /*repeat*/);
}

void Pads::setPad(atom::Pad p, atom::PadMode m, atom::Color c)
{
  int idx = p - atom::Pad::One;
  _pads[idx].Color = c;
  _pads[idx].Mode = m;

  vector<Note> notes;
  _pads[idx].toNotes(p, notes);
  _device.sendNotes(notes);
}

void Pads::startPlaying(Animation animation, bool repeat)
{
  reset();
  if (animation.size() == 0) {
    return;
  }

  _animation = move(animation);
  _repeat = repeat;
  _start = c::system_clock::now();
  step();
}

void Pads::step()
{
  if (_animation.size() == 0) {
    return;
  }

  auto now = c::system_clock::now();
  vector<Note> notes;

  while (true) {
    auto& step = _animation[_stepIndex];
    if (_start + step.Time > now) {
      break;
    }

    if (step.State.Mode == PadMode::Off) {
      auto idx = (uint8_t)step.Pad - (uint8_t)atom::Pad::One;
      _pads[idx].toNotes(step.Pad, notes);
    }
    else {
      step.State.toNotes(step.Pad, notes);
    }

    ++_stepIndex;
    if (_stepIndex >= _animation.size()) {
      if (_repeat) {
        _stepIndex = 0;
        _start = c::system_clock::now();
        break;
      }
      else {
        // TODO: would need to wait for the last frame to play before resetting
        reset();
      }
    }
  }

  if (notes.size() > 0) {
    _device.sendNotes(notes);
  }
}

void Pads::poll()
{
  step();
}

void Pads::reset()
{
  _stepIndex = 0;
  _animation.clear();
  // TODO : set initial state
}
