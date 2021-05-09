#pragma once

#include "Pads.h"

#include <stdexcept>

namespace ps
{
  class PiSample;

  /// Ensure only one component displays data on the pads at a given time.
  /// Only PiSample can grant access to the pads to other components.
  /// Access can be given and taken at any time, do not store states in the pads.
  class PadsAccess
  {
  public:
    PadsAccess(Pads& pads) : _pads(pads)
    { }

  protected:
    bool isAccessing() const { return _pads._access == this; }
    Pads& pads();

  private:
    friend class PiSample;
    void receiveAccess();
    
    virtual void onAccess() {};
    Pads& _pads;
  };

  inline void PadsAccess::receiveAccess()
  {
    if (not isAccessing()) {
      _pads._access = this;
      onAccess();
    }
    else {
      _pads._access = this;
    }
  }

  inline Pads& PadsAccess::pads()
  {
    if (not isAccessing()) {
      throw std::logic_error("Trying to get pads without being given access");
    }
    return _pads;
  }
}
