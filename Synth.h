#pragma once

#include "Atom.h"

namespace ps
{
  /// This roughly represents a business logic class for a MIDI application
  /// as it receives commands and execute them.
  class Synth
  {
  public:
    virtual ~Synth() = default;

    virtual void event(atom::Note)    = 0;
    virtual void event(atom::Control) = 0;
  };
}
