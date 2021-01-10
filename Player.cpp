#include "Player.h"

using namespace ps;
using namespace std;
namespace c = std::chrono;

#define AUDIO_OUT "audio-out-"

namespace
{
  array<int, 2> parseChannels(const string& str)
  {
    auto it = str.find(',');
    if (it == string::npos) {
      throw Exception("Invalid " AUDIO_OUT "channels parameter, expecting 'a,b'");
    }
    array<int, 2> result;
    result[0] = stoi(str.substr(0, it));
    result[1] = stoi(str.substr(it + 1));
    return result;
  }
}

ArgMap Player::args()
{
  return {
    { AUDIO_OUT "card"s, {
      .Doc = "The card on which to replay sound",
      .Value = std::nullopt
    } },
    { AUDIO_OUT "channels"s, {
      .Doc = "The channels to replay sound on (stereo).",
      .Value = "0,1"
    } },
    { AUDIO_OUT "channel-count"s, {
      .Doc = "The total number of channels on the device if it cannot be guessed",
      .Value = "-1"
    } }
  };
}

Player::Player(const ArgMap& args)
  : _interface(* args.find(AUDIO_OUT "card")->second.Value)
  , _outputChannelCount(stoi(* args.find(AUDIO_OUT "channel-count")->second.Value))
  , _channels(parseChannels(* args.find(AUDIO_OUT "channels")->second.Value))
  , _out(_interface, SND_PCM_STREAM_PLAYBACK, _outputChannelCount, _channels)
{
  if (_outputChannelCount == -1) {
    _outputChannelCount = _out.Format.Channels;
  }

  fmt::print("Will output sounds at rate={}, bits={}, total available channels {}\n",
      _out.Format.Rate, _out.Format.Bits, _outputChannelCount);

  _thread = thread([this]{ run(); });
}

Player::~Player()
{
  _stop = true;
  if (_thread.joinable()) {
    _thread.join();
  }
}

void Player::run()
{
  while (!_stop) {
    this_thread::sleep_for(c::milliseconds(1));
  }
}

int Player::load(FrameFormat, std::vector<uint8_t>&& /*bytes*/)
{
  return -1;
}

int Player::load(/* some file name */)
{
  return -1;
}
