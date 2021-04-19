#include "Recorder.h"
#include "Alsa.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>

using namespace std;
using namespace ps;
using namespace atom;

namespace c = std::chrono;

#define AUDIO_IN "audio-in-"

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
      throw Exception("Invalid " AUDIO_IN "channels parameter, expecting 'a,b'");
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
    { AUDIO_IN "card"s, {
      .Doc = "The card to record from",
      .Value = nullopt,
    } },
    { AUDIO_IN "channels"s, {
      .Doc = "Channels for recording, comma separated. Defaults to the first two",
      .Value = "0,1",
    } },
    { AUDIO_IN "channel-count"s, {
      .Doc = "When using Pulse or other audio sources that do not give any "
      "channel map, provide a way to specify the input channel count.",
      .Value = "-1"
    } },
    { AUDIO_IN "record-dir"s, {
      .Doc = "Directory where to save recorded files",
      .Value = "./"
    } }
  };
}

Recorder::Recorder(Device& d, const ArgMap& args)
  : _device(d)
  , _recordDir(* args.find(AUDIO_IN "record-dir")->second.Value)
  , _interface(* args.find(AUDIO_IN "card")->second.Value)
  , _inputChannelCount(stoi(* args.find(AUDIO_IN "channel-count")->second.Value))
  , _channels(parseChannels(* args.find(AUDIO_IN "channels")->second.Value))
  , _in(_interface, SND_PCM_STREAM_CAPTURE, _inputChannelCount, _channels)
  , _storageBytes(storageBytes(_in.Format.Bits))
{
  if (not _recordDir.empty() && _recordDir.back() != '/') {
    filesystem::directory_entry dir(_recordDir);
    if (not dir.exists() or not dir.is_directory()) {
      throw Exception("Configured " AUDIO_IN "record-dir '{}' does not exist or "
          "is not a directory", _recordDir);
    }
    // TODO : check writable by us, not much of a concern on a PI.
    _recordDir += '/';
  }

  if (_inputChannelCount == -1) {
    _inputChannelCount = _in.Format.Channels;
  }

  int sampleCount = _in.Format.Rate / 10;

  _readBuf.resize(sampleCount * _storageBytes * _inputChannelCount);
  // flac always take in 24 bit samples padded to 32 bits and always
  // 2 channels.
  _convBuf.resize(sampleCount * sizeof(uint32_t) * _channels.size());
  fmt::print("[REC] input channels: {}, rate: {}, sample bits: {} (stored: {})\n",
    _inputChannelCount, _in.Format.Rate, _in.Format.Bits, _storageBytes * 8);

  fmt::print("[REC] Recording channels {},{} on {}\n",
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
    stopRecording(true /*drain*/);
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
  auto fileName = _recordDir + filenameForTime(c::system_clock::now());

  _enc.reset(FLAC__stream_encoder_new());
  // we are receiving N channels but always keep 2, no plans to support mono
  FLAC__stream_encoder_set_channels(_enc.get(), _channels.size());
  // We may get more precision in and convert down to 24 bit as FLAC
  // can’t support more than that.
  FLAC__stream_encoder_set_bits_per_sample(
      _enc.get(),
      min(24, _in.Format.Bits));
  FLAC__stream_encoder_set_sample_rate(_enc.get(), _in.Format.Rate);
  auto res = FLAC__stream_encoder_init_file(
      _enc.get(),
      fileName.c_str(),
      nullptr,
      nullptr
  );
  if (res != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
    throw Exception("Could not initialize FLAC encoder ({})", (int)res);
  }

  // int alsaRes = snd_pcm_start(_in.Ptr);
  // if (alsaRes < 0) {
  //   throw Exception("Could not start recording ({})", AlsaErr{res});
  // }
  fmt::print("[REC] Starting to record to {}\n", fileName);
}

void Recorder::stopRecording(bool drain)
{
  if (drain) {
    snd_pcm_drain(_in.Ptr);
    recordFrames();
  }
  if (_enc) {
    FLAC__stream_encoder_finish(_enc.get());
  }
  _enc.reset();
  fmt::print("[REC] Stopped recording (ok: {}, errors: {})\n",
    _readOk, _readErrors);
  cout.flush();
  _readErrors = 0;
}

void Recorder::recordFrames()
{
  // blocking while buffer is not full, using 100ms buffer size
  // (i.e sample rate/10)
  auto nFrames = snd_pcm_readi(_in.Ptr, _readBuf.data(),
      _readBuf.size() / _inputChannelCount / _storageBytes);
  
  if (nFrames < 0) {
    if (-nFrames == EAGAIN) {
      this_thread::sleep_for(c::microseconds(500));
      return;
    }
    if (_readErrors == 0) {
      fmt::print("[REC] Read error: {} ({})\n",
          AlsaErr{nFrames}, nFrames);
    }
    ++_readErrors;
    int res = snd_pcm_recover(_in.Ptr, nFrames, true /*silent*/);
    if (res < 0) {
      fmt::print("[REC] Failed to recover after recording error, stopping...\n");
      stopRecording(false /* no drain, stuff failed */);
      return;
    }
  }
  else {
    if (nFrames == 0) {
      this_thread::sleep_for(c::microseconds(500));
      return;
    }

    ++_readOk;

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
          _in.Format.Bits * 8
        );
        if (_in.Format.Bits == 32) {
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
        stopRecording(true /*drain*/);
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

  fmt::print("[REC] Record thread exit\n");
}
