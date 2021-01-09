#include "Recorder.h"
#include "Alsa.h"

#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;
using namespace ps;
using namespace atom;

namespace c = std::chrono;

namespace 
{
  string filenameForTime(const c::system_clock::time_point& time)
  {
    time_t tt = c::system_clock::to_time_t(time);
    tm local = *std::localtime(&tt);
    auto epoch = time.time_since_epoch();
    int64_t millis = c::duration_cast<c::milliseconds>(epoch).count() % 1'000;
    return fmt::format("{}.{:03}.flac", put_time(&local, "%Y%m%d-%H%M%S"), millis);
  }

  int formatBits(snd_pcm_format_t format)
  {
    switch (format) {
      case SND_PCM_FORMAT_S16_LE: return 16;
      case SND_PCM_FORMAT_S24_LE: return 24;
      case SND_PCM_FORMAT_S32_LE: return 32;
      default:
        break;
    }
    throw Exception("Unsupported format {}", (int)format);
  }

  int storageBytes(int sampleBits)
  {
    if (sampleBits == 24) {
      return 4; // padding to int32_t
    }
    return sampleBits / 8;
  };

  array<int, 2> parseChannels(const string& str)
  {
    auto it = str.find(',');
    if (it == string::npos) {
      throw Exception("Invalid audio-in-channels parameter, expecting 'a,b'");
    }
    array<int, 2> result;
    result[0] = stoi(str.substr(0, it));
    result[1] = stoi(str.substr(it + 1));
    return result;
  }
}

unordered_map<string, Argument> Recorder::args()
{
  return {
    { "audio-in-card"s, {
      .Doc = "The card to record from",
      .Value = nullopt,
    } },
    { "audio-in-channels"s, {
      .Doc = "Channels for recording, comma separated. Defaults to the first two",
      .Value = "0,1",
    } },
    { "audio-in-channel-count", {
      .Doc = "When using Pulse or other audio sources that do not give any "
      "channel map, provide a way to specify the input channel count.",
      .Value = "-1"
    } }
  };
}

void Recorder::findCompatibleFormat()
{
  snd_pcm_t* in = nullptr;
  int err = snd_pcm_open(&in, _interface.c_str(), SND_PCM_STREAM_CAPTURE, 0);
  if (err < 0) {
    throw Exception("Could not open recording device {} : {}",
        _interface, AlsaErr{err});
  }
  PcmPtr p(in);

  // Input channel count was not given on the command line, try to guess it
  // from the channel maps. TODO: there are probably better ways.
  if (_inputChannelCount == -1) {
    auto chmaps_init = snd_pcm_query_chmaps(in);
    auto chmaps = chmaps_init;
    while (chmaps != nullptr and *chmaps != nullptr) {
      _inputChannelCount = (*chmaps)->map.channels;
      if (_inputChannelCount > max(_channels[0], _channels[1]) ) {
        break;
      }
      ++chmaps;
    }
    snd_pcm_free_chmaps(chmaps_init);
  }

  if (_inputChannelCount == -1) {
    throw Exception("Could not guess the total number of input channels. "
      "This is likely when working with pulse. You can give "
      "--audio-in-channel-count X to set the value.");
  }

  if (any_of(begin(_channels), end(_channels),
       [this](int c) { return c >= _inputChannelCount; }))
  {
    throw Exception("All channels given to --audio-in-channels must be less "
        "than the total number of channels ({})", _inputChannelCount);
  }

  _rate = 0;
  for (int rate : {48000, 44100}) {
    for (snd_pcm_format_t format:
        {SND_PCM_FORMAT_S24_LE, SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S32_LE})
    {
      int err = snd_pcm_set_params(
        p.get(),
        format,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        _inputChannelCount,
        rate,
        0 /* soft re-sample */,
        0 /* latency */);
      if (err >= 0) {
        _rate = rate;
        _format = format;
        _sampleBits = formatBits(_format);
        _storageBytes = storageBytes(_sampleBits);

        int sampleCount = _rate / 10;

        _readBuf.resize(sampleCount * _storageBytes * _inputChannelCount);
        // flac always take in 24 bit samples padded to 32 bits and always
        // 2 channels.
        _convBuf.resize(sampleCount * sizeof(uint32_t) * _channels.size());
        fmt::print("input channels: {}, rate: {}, sample bits: {} (stored: {})\n",
          _inputChannelCount, _rate, _sampleBits, _storageBytes * 8);
        return;
      }
    }
  }

  throw Exception("Device does not support any format we support (44100 or "
      "48000 sampling rate, *signed* 16, 24 or 32 bit little endian).");
}

Recorder::Recorder(Device& d, const ArgMap& args)
  : _device(d)
  , _inputChannelCount(stoi(* args.find("audio-in-channel-count")->second.Value))
  , _channels(parseChannels(* args.find("audio-in-channels")->second.Value))
  , _interface(* args.find("audio-in-card")->second.Value)
{
  findCompatibleFormat();
  fmt::print("Recording channels {},{} on {}\n",
      _channels[0], _channels[1], _interface);
  cout.flush();
  _thread = thread([this]{ run(); });
}

Recorder::~Recorder()
{
  _stop = true;
  if (_thread.joinable()) {
    _thread.join();
  }

  if (_on) {
    stopRecording();
  }
}

void Recorder::toggle()
{
  _on = not _on;
  _device.sendControl(switchButton(Buttons::Record, _on));

  if (_on) {
    _last = c::system_clock::now();
    _buttonOn = true;
  }
}

void Recorder::poll()
{
  // slow path poll - do not access audio here.

  if (!_on) {
    return;
  }

  constexpr auto blinkDuration = c::milliseconds(200);

  auto now = c::system_clock::now();
  if (_last + blinkDuration <= now) {
    _last += blinkDuration;
    _buttonOn = not _buttonOn;
    _device.sendControl(switchButton(Buttons::Record, _buttonOn));
  }
}

void Recorder::startRecording()
{
  // Setup FLAC
  auto fileName = filenameForTime(c::system_clock::now());
  _enc.reset(FLAC__stream_encoder_new());
  // we are receiving N channels but always keep 2
  FLAC__stream_encoder_set_channels(_enc.get(), _channels.size());
  // We may get more data in and convert down to 24 bit
  FLAC__stream_encoder_set_bits_per_sample(
      _enc.get(),
      min(24, _sampleBits));
  FLAC__stream_encoder_set_sample_rate(_enc.get(), _rate);
  auto res = FLAC__stream_encoder_init_file(
      _enc.get(),
      fileName.c_str(),
      nullptr,
      nullptr
  );
  if (res != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
    throw Exception("Could not initialize FLAC encoder ({})", (int)res);
  }

  // Setup ALSA
  snd_pcm_t* in = nullptr;
  int err = snd_pcm_open(&in, _interface.c_str(),
      SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
  if (err < 0) {
    throw Exception("Could not open recording device {} : {}",
        _interface, AlsaErr{err});
  }
  _in.reset(in);

  snd_pcm_nonblock(_in.get(), 1);

  err = snd_pcm_set_params(
      _in.get(),
      _format,
      SND_PCM_ACCESS_RW_INTERLEAVED,
      _inputChannelCount,
      _rate,
      0 /* soft re-sample */,
      0 /* latency */);
  if (err < 0) {
    throw Exception("Failed to set recording parameters {}",
        AlsaErr{err});
  }

  fmt::print("Starting to record to {}\n", fileName);
}

void Recorder::stopRecording()
{
  FLAC__stream_encoder_finish(_enc.get());
  _enc.reset();
  _in.reset();
  fmt::print("Stopped recording (errors: {})\n", _readErrors);
  cout.flush();
  _readErrors = 0;
}

void Recorder::recordFrames()
{
  // blocking while buffer is not full, typically using 1s buffer size
  auto nFrames = snd_pcm_readi(_in.get(), _readBuf.data(),
      _readBuf.size() / _inputChannelCount / _storageBytes);
  
  if (nFrames < 0) {
    if (-nFrames == EAGAIN) {
      this_thread::sleep_for(c::milliseconds(1));
      return;
    }
    if (_readErrors == 0) {
      fmt::print("Recorder : Read error: {} ({})\n",
          snd_strerror(nFrames), nFrames);
    }
    ++_readErrors;
    snd_pcm_recover(_in.get(), nFrames, true /*silent*/);
  }
  else {
    if (nFrames == 0) {
      this_thread::sleep_for(c::milliseconds(1));
      return;
    }

    // TODO: this shouts "vectorize me"
    for (long i = 0; i < nFrames; ++i) {
      for (long j = 0; j < 2; ++ j) {
        auto idx = i * 2 + j; // out i-th frame, j-th channel 
        _convBuf[idx] = 0;
        memcpy(
          &_convBuf[idx],
          &_readBuf[
            i * _storageBytes * _inputChannelCount + 
            _channels[j] * _storageBytes
          ],
          _sampleBits * 8
        );
        if (_sampleBits == 32) {
          _convBuf[idx] = _convBuf[idx] / 256;
        }
      }
    }

    // Let’s note that for FLAC a sample is an Alsa frame.
    FLAC__stream_encoder_process_interleaved(
        _enc.get(), (int32_t*)_convBuf.data(), nFrames);
  }
}

void Recorder::run()
{
  bool wasOn = false;

  while (not _stop) {
    if (wasOn) {
      if (!_on) {
        stopRecording();
        wasOn = false;
      }
      else {
        recordFrames();
      }
    }

    else { // was off
      if (_on) {
        startRecording();
        wasOn = true;
      }
      else {
        this_thread::sleep_for(c::milliseconds(1));
      }
    }
  }

  fmt::print("Record thread exit\n");
}
