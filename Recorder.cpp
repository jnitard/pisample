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
  FLAC__stream_encoder_set_channels(_enc.get(), 2);
  // We may get more data in and convert down to 24 bit
  FLAC__stream_encoder_set_bits_per_sample(
      _enc.get(),
      min(3, _sampleBytes) * 8);
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
  int err = snd_pcm_open(&in, _interface.c_str(), SND_PCM_STREAM_CAPTURE, 0);
  if (err < 0) {
    throw Exception("Could not open recording device {} : {}",
        _interface, AlsaErr{err});
  }
  _in.reset(in);

  auto formatByBytes = [](int bytes){
    switch (bytes) {
      case 2: 
        return SND_PCM_FORMAT_S16_LE;
      case 3:
        return SND_PCM_FORMAT_S24_LE;
      case 4:
        return SND_PCM_FORMAT_S32_LE;
      default:
        throw Exception("FLAC only support native (i.e. little endian "
          "on raspberry pi), with 16 or 32 bit per sample") ;
    }
  };

  // {
  //   cout << "chmaps" << endl;
  //   auto chmaps_init = snd_pcm_query_chmaps(_in.get());
  //   auto chmaps = chmaps_init;
  //   while (chmaps != nullptr and *chmaps != nullptr) {
  //     fmt::print("{}, {}\n", (*chmaps)->map.channels, (int)(*chmaps)->type);
  //     ++chmaps;
  //   }
  //   snd_pcm_free_chmaps(chmaps_init);
  // }

  err = snd_pcm_nonblock(_in.get(), true);
  if (err < 0) {
    throw Exception("Failed to set non-blocking mode on recording device",
        AlsaErr{err});
  }

  err = snd_pcm_set_params(
      _in.get(),
      formatByBytes(_sampleBytes),
      SND_PCM_ACCESS_RW_INTERLEAVED,
      _channels,
      _rate,
      1 /* soft re-sample */,
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
  _readErrors = 0;
}

void Recorder::recordFrames()
{
  // blocking while buffer is not full, typically using 1s buffer size
  auto nFrames = snd_pcm_readi(_in.get(), _readBuf.data(),
      _readBuf.size() / _channels / _storageBytes);
  
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
    snd_pcm_recover(_in.get(), nFrames, false /*silent*/);
  }
  else {
    if (nFrames == 0) {
      this_thread::sleep_for(c::milliseconds(1));
      return;
    }

    // TODO: this shouts "vectorize me"
    for (long i = 0; i < nFrames; ++i) {
      for (long j = 0; j < 2; ++ j) {
        // _sampleBytes * 8 bits to 32, assuming LE native
        _convBuf[i * 2 + j] = 0;
        memcpy(
          &_convBuf[i * 2 + j],
          &_readBuf[ // TODO: need to convert 32->24 at times
            i * _storageBytes * _channels + 
            (j+8) * _storageBytes
          ],
          _sampleBytes);
      }
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
