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
    return fmt::format("{}.{}.flac", put_time(&local, "%Y%m%d-%H%M%S"), millis);
  }
}

Recorder::Recorder(Device& d, const char* interface, const char* /* file */)
  : _device(d)
  , _interface(interface)
{
  _thread = std::thread([this]{ run(); });
}

Recorder::~Recorder()
{
  _stop = true;
  if (_thread.joinable()) {
    _thread.join();
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
  FLAC__stream_encoder_set_channels(_enc.get(), _channels);
  FLAC__stream_encoder_set_bits_per_sample(_enc.get(), _sampleBytes * 8);
  FLAC__stream_encoder_set_sample_rate(_enc.get(), _rate);
  auto res = FLAC__stream_encoder_init_file(
      _enc.get(),
      fileName.c_str(),
      nullptr,
      nullptr
  );
  if (res != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
    throw FormattedException("Could not initialize FLAC encoder ({})",
        (int)res);
  }

  // Setup ALSA
  int err = snd_pcm_open(&_in, _interface.c_str(), SND_PCM_STREAM_CAPTURE, 0);
  if (err < 0) {
    throw FormattedException("Could not open recording device {} : {}",
        _interface, AlsaErr{err});
  }

  auto formatByBytes = [](int bytes){
    switch (bytes) {
      case 2: 
        return SND_PCM_FORMAT_S16_LE;
      case 3:
        return SND_PCM_FORMAT_S24_LE;
      default:
        throw invalid_argument("FLAC only support native (i.e. little endian "
          "on raspberry pi), with 16 or 32 bit per sample") ;
    }
  };

  err = snd_pcm_set_params(
      _in,
      formatByBytes(_sampleBytes),
      SND_PCM_ACCESS_RW_INTERLEAVED,
      _channels,
      _rate,
      1 /* soft re-sample */,
      0 /* latency */);
  if (err < 0) {
    throw FormattedException("Failed to set recording parameters {}",
        AlsaErr{err});
  }

  fmt::print("Starting to record to {}\n", fileName);
}

void Recorder::stopRecording()
{
  FLAC__stream_encoder_finish(_enc.get());
  _enc.reset();
  snd_pcm_drop(_in);
  snd_pcm_close(_in);
  fmt::print("Stopped recording (errors: {})\n", _readErrors);
  _readErrors = 0;
}

void Recorder::recordFrames()
{
  // blocking while buffer is not full, typically using 1s buffer size
  auto nFrames = snd_pcm_readi(_in, _readBuf.data(),
      _readBuf.size() / _channels / _storageBytes);
  
  if (nFrames < 0) {
    if (_readErrors == 0) {
      fmt::print("Recorder : Read error: {} ({})\n",
          snd_strerror(nFrames), nFrames);
    }
    ++_readErrors;
    snd_pcm_recover(_in, nFrames, true /*silent*/);
  }
  else {
    if (nFrames == 0) {
      this_thread::sleep_for(c::milliseconds(1));
      return;
    }

    // TODO: this shouts "vectorize me"
    for (long i = 0; i < nFrames * _channels; ++i) {
       // _sampleBytes * 8 bits to 32, assuming LE native
      _convBuf[i] = 0;
      memcpy(&_convBuf[i], &_readBuf[i * _storageBytes], _sampleBytes);
    }

    // Let’s note that for FLAC a sample is an Alsa frame.
    FLAC__stream_encoder_process_interleaved(
        _enc.get(), _convBuf.data(), nFrames);
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
}