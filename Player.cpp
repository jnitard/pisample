#include "Player.h"
#include "Log.h"

extern "C" {
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
}

using namespace ps;
using namespace std;
namespace c = std::chrono;

#define AUDIO_OUT "audio-out-"

namespace
{
  auto logger = Log("PLAY");

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

  /// Create an audio codec from the "best" stream.
  /// TODO (C-ism): returns 0 on success
  /// Blantanly stolen from ffmpeg’s examples/demuxing_decoding.c "open_codec_context".
  int open_audio_codec(
      const filesystem::path& filename,
      AVFormatContext& formats,
      int& stream_index,
      AVCodecContext*& decoder) // TODO: leaked ?
  {
    stream_index = -1;
    decoder = nullptr;

    int ret = av_find_best_stream(&formats, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (ret < 0) {
      logger.error("Could not find any stream in {}", filename);
      return ret;
    }
    else {
      stream_index = ret;
      AVStream* st = formats.streams[stream_index];

      /* find decoder for the stream */
      const AVCodec* dec = avcodec_find_decoder(st->codecpar->codec_id); // TODO leaked ?
      if (not dec) {
        logger.error("Audio codec unknown to ffmpeg");
        return AVERROR(EINVAL);
      }

      /* Allocate a codec context for the decoder */
      decoder = avcodec_alloc_context3(dec);
      if (decoder == nullptr) {
        logger.error("Failed to allocate audio context (out of memory ?)");
        return AVERROR(ENOMEM);
      }

      /* Copy codec parameters from input stream to output codec context */
      if ((ret = avcodec_parameters_to_context(decoder, st->codecpar)) < 0) {
        logger.error("Failed to copy audio parameters (out of memory ?)");
        return ret;
      }

      /* Init the decoders */
      AVDictionary* opts = nullptr; // TODO: leaked ?
      if ((ret = avcodec_open2(decoder, dec, &opts)) < 0) {
        logger.error("Failed to open audio codec");
        return ret;
      }
    }

    return 0;
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

Player::Player(const ArgMap& args, Pads& pads)
  : PadsAccess(pads)
  , _interface(* args.find(AUDIO_OUT "card")->second.Value)
  , _outputChannelCount(stoi(* args.find(AUDIO_OUT "channel-count")->second.Value))
  , _channels(parseChannels(* args.find(AUDIO_OUT "channels")->second.Value))
  , _out(_interface, SND_PCM_STREAM_PLAYBACK, _outputChannelCount, _channels)
{
  if (_outputChannelCount == -1) {
    _outputChannelCount = _out.Format.Channels;
  }

  logger.info("Will output sounds at rate={}, bits={}, total available "
      "channels {}\n", _out.Format.Rate, _out.Format.Bits, _outputChannelCount);

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

int Player::load(std::filesystem::path path)
{
  AVFormatContext* formats = nullptr; // TODO leaked ?
  if (avformat_open_input(&formats, path.c_str(), nullptr, nullptr) < 0) {
    logger.error("Unable to open {}", path);
    return -1;
  }

  AVCodecContext* codec = nullptr;
  int streamIndex = 0;
  if (open_audio_codec(path, *formats, streamIndex, codec) < 0) {
    logger.error("Unable to get streams for {}", path);
    return -1;
  }

  return -1;
}
