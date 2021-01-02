#include <iostream>
#include <utility>
#include <memory>
#include <chrono>
#include <thread>
#include <csignal>
#include <optional>
#include <sstream>
#include <unordered_map>

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


atomic<bool> goOn = true;
atomic<bool> firstSig = true;
void signalHandler(int)
{
  if (not firstSig) {
    terminate();
  }
  firstSig = true;
  cerr << "Got signal, stopping\n";
  goOn = false;
}

void setupSignals()
{
  struct sigaction action;
  action.sa_handler = &signalHandler;
  action.sa_mask = sigset_t{};
  action.sa_flags = 0;
  action.sa_restorer = nullptr;
  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGSTOP, &action, nullptr);
  sigaction(SIGKILL, &action, nullptr);
  sigaction(SIGHUP, &action, nullptr);
  sigaction(SIGABRT, &action, nullptr);
}


class PiSample : public Synth
{
public:
  PiSample(Device& device, Recorder& recorder) :
      _device(device),
      _recorder(recorder)
  { }

  ~PiSample()
  {
    _device.sendControl(switchButton(Buttons::Stop, false));
    _device.sendControl(switchButton(Buttons::Shift, false));
  }

  bool getShutdown() const { return _willShutdown; }

private:
  void event(Note n) override
  {
    cout << "Note " << (n.OnOff ? "on" : "off")
         << ", channel: " << (int)n.Channel
         << ", note: " << (int)n.Note
         << ", velocity: " << (int)n.Velocity
         << "\n";
  }

  void event(Control c) override
  {
    switch (static_cast<Buttons>(c.Param)) {
      case Buttons::Record:
        if (c.Value == 0x00) {
          _recorder.toggle();
        }
        break;
      case Buttons::Shift:
        _shiftPressed = (c.Value != 0);
        _device.sendControl(switchButton(Buttons::Shift, _shiftPressed));
        _device.sendControl(switchButton(Buttons::Stop, _shiftPressed));
        break;
      case Buttons::Stop:
        if (_shiftPressed) {
          goOn = false;
          _willShutdown = true;
        }
        break;
      default:
        cout << "Control: "
             << ", channel: 0" // TODO : for now always 0, most likely need
             << ", param: " << (int)c.Param // something not an ATOM
             << ", value: " << (int)c.Value;
        break;
    }
  }

  Device& _device;
  Recorder& _recorder;

  bool _shiftPressed = false;
  bool _stopPressed = false;
  bool _willShutdown = false;
};


struct Argument
{
  string Doc;
  optional<string> Value;
  bool Flag = false;
  bool Given = false;
};

void printHelp(const unordered_map<string, Argument>& args)
{
  for (auto& [name, arg]: args) {
    if (arg.Flag) {
      fmt::print("--{} -- {}\n", name, arg.Doc);
    }
    else {
      fmt::print("--{} [{}] -- {}\n", name,
          arg.Value.value_or("required"), arg.Doc);
    }
  }
  cout.flush();
  exit(EXIT_FAILURE);
}

void readArguments(
    unordered_map<string, Argument>& args,
    int argc, const char* const* argv)
{
  int i = 0; // modified if we find a value.
  while (++i, i < argc) {
    if (argv[i] == "--help"s || argv[i] == "-h"s) {
        printHelp(args);
        __builtin_unreachable();
    }

    bool isArg = string(argv[i]).substr(0, 2) == "--";
    if (!isArg) {
      if (i == 1) {
        throw Exception("Expecting --arg");
      }
      continue;
    }

    auto argName = string(argv[i]).substr(2);
    auto it = args.find(argName);
    if (it == end(args)) {
      throw Exception("Unknown argument: {}", argName);
    }

    auto& arg = it->second;
    if (arg.Given) {
      throw Exception("{} was given twice", it->first);
    }
    arg.Given = true;

    if (i+1 < argc) {
      bool nextIsArg = string(argv[i+1]).substr(0, 2) == "--";
      if (arg.Flag) {
        if (!nextIsArg) {
          throw Exception("{} is a flag but wass given a value ({})",
            it->first, argv[i+1]);
        }
        else {
          arg.Value = "true";
        }
      }
      else { // not a flag, needs a value
        if (! nextIsArg) {
          arg.Value = argv[i+1];
          ++i;
        }
        else {
          throw Exception("{} expects a value but none given", it->first);
        }
      }
    }
    // last argument
    else if (arg.Flag) {
      arg.Value = "true";
    }
    else {
      throw Exception("{} expects a value but none given", it->first);
    }
  }

  for (auto& [name, arg]: args) {
    if (arg.Value == nullopt) {
      throw Exception("No value given for {} and no default exists", name);
    }
  }
}


int main(int argc, const char* const* argv)
try {
  setupSignals();

  unordered_map<string, Argument> args = {
    // { "config"s, {
    //   .Doc = "A config file with further options in it.",
    //   .Value = "",
    // } },
    { "midi-in-port"s, {
      .Doc = "The midi port for the ATOM device (eg 'ATOM MIDI 1')",
      .Value = nullopt,
    } },
    { "audio-in-card"s, {
      .Doc = "The card to record from",
      .Value = "pulse",
    } },
    { "audio-in-channels"s, {
      .Doc = "Select channels for recording",
      .Value = "",
    } },
    { "record"s, {
      .Doc = "Start recording on startup. Meant for testing mainly.",
      .Value = "false",
      .Flag = true,
    } }
  };

  readArguments(args, argc, argv);

  const char* devicePortName = args["midi-in-port"].Value->c_str();
  const char* recordCard = args["audio-in-card"].Value->c_str();
  bool recordOnStart;
  stringstream(args["record"].Value->c_str()) >> boolalpha >> recordOnStart;

  bool shutdown = false;
  {
    Device device(devicePortName);
    Pads pads(device);
    Recorder recorder(device, recordCard, nullptr);
    PiSample piSample(device, recorder);

    device.setSynth(piSample);

    if (recordOnStart) {
      recorder.toggle();
    }

    while (goOn) {
      if (not device.poll()) {
        pads.poll();
        recorder.poll();
        this_thread::sleep_for(c::microseconds(500));
      }
    }

    shutdown = piSample.getShutdown();
  }

  // Obvioulsy this is really realistic on a PI only.
  if (shutdown) {
    system("sudo shutdown now");
  }

  return EXIT_SUCCESS;
}
catch (const exception& ex) {
  cerr << ex.what() << '\n';
  return EXIT_FAILURE;
}
