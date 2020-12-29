#include <iostream>
#include <utility>
#include <memory>
#include <chrono>
#include <thread>

#include <alsa/asoundlib.h>
// #include <wx/wx.h>

#include "Alsa.h"
#include "Atom.h"
#include "Pads.h"
#include "Device.h"
#include "Recorder.h"

using namespace ps; // PiSample
using namespace atom;
using namespace std;
namespace c = std::chrono;
using namespace std::string_literals;


class PiSample : public Synth
{
public:
  PiSample(Recorder& recorder) : _recorder(recorder)
  { }

private:
  void event(Note n) override
  {
    cout << "Note " << (n.OnOff ? "on" : "off")
         << ", channel: " << (int)n.Channel
         << ", note: " << (int)n.Note
         << ", velocity: " << (int)n.Velocity;
  }
  void event(Control c) override
  {
    switch (static_cast<Buttons>(c.Param)) {
      case Buttons::Record:
        if (c.Value == 0x7f) {
          _recorder.toggle();
        }
        break;
      default:
        cout << "Control: "
             << ", channel: 0"
             << ", param: " << (int)c.Param
             << ", value: " << (int)c.Value;
        break;
    }

  }

  Recorder& _recorder;
};


int main(int argc, const char* const* argv)
{
  const char* devicePortName = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == "--port"s) {
      if (devicePortName != nullptr) {
        cerr << "--port given several times\n";
        return EXIT_FAILURE;
      }
      if (i + 1 >= argc) {
        cerr << "--port given no values\n";
        return EXIT_FAILURE;
      }
      devicePortName = argv[i + 1];
    }
  }

  if (devicePortName == nullptr) {
    cerr << "--port is required\n";
    return EXIT_FAILURE;
  }

  Device device(devicePortName);
  Pads pads(device);
  Recorder recorder(device, nullptr, nullptr);
  
  PiSample piSample(recorder);

  device.setSynth(piSample);


  while (true) {
    if (not device.poll()) {
      pads.poll();
      this_thread::sleep_for(c::microseconds(100));
    }
  }

  return EXIT_SUCCESS;
}

