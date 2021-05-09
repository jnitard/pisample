#include "PiSample.h"
#include "Strings.h"
#include "Log.h"

#include <iostream>
#include <utility>
#include <stdexcept>

using namespace atom;
using namespace ps;
using namespace std;

namespace {
  auto logger = Log("UI");
}

void PiSample::finalizeSample(optional<Sample>& sample, std::filesystem::path file)
{
  auto& s = sample.value();
  s.PlayerIndex = _player.load(file);
  if (s.PlayerIndex < 0) {
    logger.warn("Ignoring sample {} - player could not load it", s.Name);
    sample = nullopt;
  }
}

void PiSample::loadBanks(string_view fileName)
{
  ifstream in(static_cast<string>(fileName));
  if (not in) {
    logger.throw_("Could not open sample files '{}' for reading. You can "
        "pass an empty file if you do not wish to load any.", fileName);
  }

  optional<Sample>* currentSample = nullptr;

  vector<string_view> parts;
  string rawLine;
  int index = 0;
  string currentFile;
  int sampleCount = 0;
  int currentBank = 0;

  while (getline(in, rawLine)) {
    ++index;
    string_view line = trim(rawLine);
    if (line.empty() || line[0] == '#') {
      continue;
    }

    try { // to append some common info to any exception encountered.
      if (line[0] == '[') {
        if (currentSample != nullptr) {
          finalizeSample(*currentSample, currentFile);
        }

        // new sample
        if (line.back() != ']') {
          logger.throw_("Malformed line: missing closing ']'");
        }
        line = line.substr(1, line.size() - 2);

        split(parts, line, '.');
        if (parts.size() != 2 or
            parts[0].substr(0, 4) != "Bank" or
            parts[1].substr(0, 3) != "Pad")
        {
          logger.throw_("[Malformed line: Must be [BankX.PadY]");
        }

        parts[0] = parts[0].substr(4); // remove "Bank"
        parts[1] = parts[1].substr(3); // remove "Pad"

        int bankNo = -1;
        try {
          bankNo = svtoi(parts[0]);
          if (bankNo < 1) {
            throw out_of_range(">0");
          }
        }
        catch (...) {
          logger.throw_("Could not convert {} to a valid bank number (> 0)",
              parts[0]);
        }
        if (bankNo != currentBank and bankNo != currentBank + 1) {
          logger.throw_("Bank is {} but last was {} - expecting the same "
            "value or {}. Banks must be given in order in the file.",
            bankNo, currentBank, currentBank + 1);
        }
        currentBank = bankNo;
        if (_banks.size() < static_cast<unsigned>(bankNo)) {
          _banks.resize(bankNo);
          // filled with unset Sample for pads.
        }

        int padNo = -1;
        try {
          padNo = svtoi(parts[1]);
          if (padNo < 1 or padNo > 16) {
            throw out_of_range(">0 && <17");
          }
        }
        catch (...) {
          logger.throw_("Could not convert {} to a valid pad number "
              "(1-16)", parts[1]);
        }

        if (_banks.back()[padNo - 1].has_value()) {
          logger.throw_("Bank{}.Pad{} is present twice in the config",
              bankNo, padNo);
        }
        currentSample = & _banks.back()[padNo - 1];
        *currentSample = Sample(); // may be reset later in case on an error.
        ++sampleCount;
      }

      else {
        if (currentSample == nullptr) {
          logger.throw_("A property key=value was given before "
              "[BankY.PadY]");
        }
        split(parts, line, '=');
        if (parts.size() != 2 or parts[0].empty() or parts[1].empty()) {
          logger.throw_("Expecting key=value but found {}. No '=' "
              "allowed in the value.", line);
        }

        // TODO: check for repeated keys.
        if (parts[0] == "Name") {
          currentSample->value().Name = parts[1];
        }
        else if (parts[0] == "File") {
          currentFile = parts[1];
        }
        else if (parts[0] == "Color") {
          if (parts[1][0] != '#') { // note we checked it’s not empty above
            logger.throw_("At line {} expecting an hex color formatted as "
                "'#123456'", index);
          }
          currentSample->value().Color = atom::Color::fromString(parts[1]);
        }
      }
    }
    catch (const exception& ex) {
      logger.throw_("samples file {}, line {}: {}", fileName, index, ex.what());
    }
  }

  if (currentSample == nullptr) {
    logger.info("No samples found in samples line {}", fileName);
    return;
  }

  finalizeSample(*currentSample, currentFile);

  logger.info("Loaded {} banks and {} samples, read {} lines in {}",
      currentBank, sampleCount, index, fileName);
}


PiSample::PiSample(
  const unordered_map<string, Argument>& args,
  Device& device,
  Pads& pads,
  Recorder& recorder,
  Player& player)
  : PadsAccess(pads)
  , _device(device)
  , _recorder(recorder)
  , _player(player)
{
  loadBanks(*args.find("samples")->second.Value);

  _currentView = 1;
  cycleView(false); // activate ourselves as the pads accessor.
}

PiSample::~PiSample()
{
  _device.sendControl(atom::switchButton(atom::Buttons::Stop, false));
  _device.sendControl(atom::switchButton(atom::Buttons::Shift, false));
}

void PiSample::event(atom::Note n)
{
  cout << "Note " << (n.OnOff ? "on" : "off")
       << ", channel: " << (int)n.Channel
       << ", note: " << (int)n.Note
       << ", velocity: " << (int)n.Velocity
       << "\n";
}

void PiSample::event(atom::Control c)
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
        _willShutdown = true;
      }
      break;
    case Buttons::Up:
      if (c.Value == 0) { // release
       cycleView(true);
      }
      break;
    case Buttons::Down:
      if (c.Value == 0) { // release
        cycleView(false);
      }
      break;

    default:
      cout << "Control: "
            << ", channel: 0" // TODO : for now always 0, most likely need
            << ", param: " << (int)c.Param // something not an ATOM
            << ", value: " << (int)c.Value
            << '\n';
      break;
  }
}

void PiSample::cycleView(bool next)
{
  if (next) {
    _currentView = (_currentView + 1) % _views.size();
  }
  else {
    _currentView =  (_currentView - 1 + _views.size()) % _views.size();
  }

  // First display where we are going, before actually changing anything
  this->receiveAccess();
  pads().reset();
  pads().setPad(
    atom::Pad::One + _currentView,
    atom::PadMode::On,
    _viewColors[_currentView]
  );
  _viewAnimTimeout = c::milliseconds(500) + c::system_clock::now();
}

void PiSample::poll()
{
  if (_viewAnimTimeout < c::system_clock::now()) {
    _viewAnimTimeout = _viewAnimTimeout.max();
    _views[_currentView]->receiveAccess();
  }

  if (not PadsAccess::isAccessing()) {
    return;
  }

  if (_banks.size() == 0) {
    return;
  }
}

void PiSample::onAccess()
{
  if (_banks.size() == 0) {
    // if we don’t have any sample, play an animation
    pads().startPlaying(caterpillar(), true);
    return;
  }
  std::cout << "access" << std::endl;

  bool allOff = true;
  for (unsigned i = 0; i < _banks[_currentBank].size(); ++i) {
    auto& p = _banks[_currentBank][i];
    if (p.has_value()) {
      std::cout << "coin " << i << std::endl;
      pads().setPad(
        atom::Pad::One + i,
        atom::PadMode::On,
        p->Color
      );
      allOff = false;
    }
    else {
      pads().setPad(
        atom::Pad::One + i,
        atom::PadMode::Off,
        atom::Color{}
      );
    }
  }

  if (allOff) {
    pads().startPlaying(caterpillar(), true);
  }
}