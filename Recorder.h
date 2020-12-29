#pragma once

#include "Device.h"

namespace ps
{
  class Recorder
  {
  public:
    Recorder(Device& d, const char* /*interface*/, const char* /* file */)
      : _device(d)
    { }

    void toggle();

  private:
    Device& _device;
    bool _on = false;
  };
}