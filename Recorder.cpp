#include "Recorder.h"

using namespace ps;
using namespace atom;

void Recorder::toggle()
{
  _on = not _on;
  _device.sendControl(switchButton(Buttons::Record, _on));

  if (_on) {
    fmt::print("Starting to record\n");
  }
  else {
    fmt::print("Stopped recording\n");
  }
}

