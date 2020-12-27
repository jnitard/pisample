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

using namespace ps; // PiSample

using namespace std;
namespace c = std::chrono;
using namespace std::string_literals;


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

  while (true) {
    if (not device.poll()) {
      pads.poll();
      this_thread::sleep_for(c::microseconds(100));
    }
  }

  return EXIT_SUCCESS;
}

