#pragma once

#include "Device.h"

#include <alsa/asoundlib.h>
#include <FLAC/stream_encoder.h>

#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>

namespace ps
{
  struct FlacDeleter
  {
    void operator()(FLAC__StreamEncoder* enc)
    {
      if (enc) {
        FLAC__stream_encoder_delete(enc);
      }
    }
  };


  /// Only really supporting 2 or 3 bytes per samples right now (16 or 24 bits).
  /// For storing 24 we need 32 bits of storage though.
  static constexpr int storageBytes(int sampleBytes)
  {
    if (sampleBytes == 3) {
      return 4;
    }
    return sampleBytes;
  };

  using FlacPtr = std::unique_ptr<FLAC__StreamEncoder, FlacDeleter>;

  /// This is meant to record a full DJ set to disk rather than a sample.
  class Recorder
  {
  public:
    Recorder(Device& d, const char* /*interface*/, const char* /* file */);
    ~Recorder();

    /// start or stop recording, when the record button is pressed
    void toggle();

    void poll();

  private:
    Device& _device;
    std::atomic<bool> _on   = false;
    std::atomic<bool> _stop = false;

    void startRecording();
    void stopRecording();
    void recordFrames();
    void run();
    std::thread _thread;

    static constexpr int _rate = 44100;
    static constexpr int _channels = 2;
    static constexpr int _sampleBytes = 2;
    static constexpr int _storageBytes = storageBytes(_sampleBytes);

    // **** these variables should only be accessed in the rec thread ****
    snd_pcm_t* _in = nullptr;
    FlacPtr _enc;
    std::array<uint8_t, _rate * _channels * _storageBytes> _readBuf = {};
    // flac always take int32_t i.e. signed 32 bit values
    std::array<int32_t, _rate * _channels> _convBuf = {};
    std::string _interface;
    size_t _readErrors = 0;
    // **** --------------------------------------------------------- ****

    std::chrono::system_clock::time_point _last = {};
    bool _buttonOn = true;
  };
}