#pragma once

#include "Device.h"
#include "Player.h"
#include "Recorder.h"
#include "Alsa.h"

#include <filesystem>

namespace ps
{

/// Business logic for the application.
class PiSample : public Synth
{
public:
  static std::unordered_map<std::string, Argument> args()
  {
    return {
      { "samples", {
        .Doc = "An .ini with the list of samples and pages",
        .Value = std::nullopt,
      } }
    };
  }

  PiSample(
    const std::unordered_map<std::string, Argument>& args,
    Device& device, Recorder& recorder, Player& player
  );

  /// Tell other components to shutdown
  ~PiSample();

  bool shutdown() const { return _willShutdown; }

private:
  /// A sample and its representation on a pad.
  struct Sample
  {
    std::string Name;
    int PlayerIndex = -1;
    atom::Color Color;
  };

  void event(atom::Note n) override;
  void event(atom::Control c) override;

  void finalizeSample(std::optional<Sample>& sample, std::filesystem::path file);
  void loadBanks(std::string_view filename);

  Device& _device;
  Recorder& _recorder;
  Player&  _player;

  /// A "page" of samples - i.e. 16 pads worth off.
  using Bank = std::array<std::optional<Sample>, 16>;

  std::vector<Bank> banks_;

  bool _shiftPressed = false;
  bool _stopPressed = false;
  bool _willShutdown = false;
};

}