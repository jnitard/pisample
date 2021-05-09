#pragma once

#include "Alsa.h"
#include "Device.h"
#include "PadsAccess.h"
#include "Player.h"
#include "Recorder.h"

#include <filesystem>

namespace ps
{

/// Business logic for the application.
class PiSample : public Synth, public PadsAccess
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
    Device& device,
    Pads& pads,
    Recorder& recorder,
    Player& player);

  /// Tell other components to shutdown
  ~PiSample();

  bool shutdown() const { return _willShutdown; }

  /// Manage background task or any animation that may be running
  void poll();

private:
  /// A sample and its representation on a pad.
  struct Sample
  {
    std::string Name;
    int PlayerIndex = -1;
    atom::Color Color;
  };

  enum class Views
  {
    Sampler,
    Player,
    Recorder,

    LastView,
  };

  inline static constexpr
  std::array<atom::Color, (unsigned)Views::LastView> _viewColors =
  {
    atom::Color{.r = 0x7f, .g = 0x00, .b = 0x00},
    atom::Color{.r = 0x00, .g = 0x7f, .b = 0x00},
    atom::Color{.r = 0x00, .g = 0x00, .b = 0x7F}
  };
  static_assert(_viewColors.size() == (unsigned)Views::LastView);

  void event(atom::Note n) override;
  void event(atom::Control c) override;

  void cycleView(bool next);

  void finalizeSample(std::optional<Sample>& sample, std::filesystem::path file);
  void loadBanks(std::string_view filename);

  void onAccess() override;

  Device& _device;
  Recorder& _recorder;
  Player&  _player;

  /// A "page" of samples - i.e. 16 pads worth off.
  using Bank = std::array<std::optional<Sample>, 16>;

  std::vector<Bank> _banks;
  int _currentBank = 0;

  std::array<PadsAccess*, (unsigned)Views::LastView> _views = {
    this, &_recorder, &_player
  };
  int _currentView;
  // When the display is changed to another component, we first give an indication
  // of that is selected for half a second before actually changing the display.
  // NOTE: I did not find another location to give that indication without going
  // to a screen. We could for instance use the top/bottom/left/right buttons
  // but that’s not terribly extensible.
  c::system_clock::time_point _viewAnimTimeout;

  bool _shiftPressed = false;
  bool _stopPressed = false;
  bool _willShutdown = false;
};

}