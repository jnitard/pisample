#pragma once

#include "Device.h"
#include "Alsa.h"
#include "Arguments.h"

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
  using FlacPtr = std::unique_ptr<FLAC__StreamEncoder, FlacDeleter>;


  /// This is meant to record a full DJ set to disk rather than a sample.
  /// TODO: record to memory for live sampling.
  class Recorder
  {
  public:
    /// Note: The device is used to affect some buttons
    /// TODO: could we break the dependency ? not worth it for now.
    Recorder(Device& d, const ArgMap&);
    Recorder(const Recorder&) = delete;
    ~Recorder();

    static ArgMap args();

    /// start or stop recording, when the record button is pressed
    void toggle();

    void poll();

  private:
    std::atomic<bool> _on   = false;
    std::atomic<bool> _stop = false;


    void findCompatibleFormat();

    void startRecording();
    void stopRecording(bool drain);
    void recordFrames();
    void run();

    std::thread _thread;

    Device& _device;

    // ----- these variables should only be accessed in the rec thread ------ //
    // (they are set in the constructor)

    static constexpr int _outputChannelCount = 2;
    std::string _recordDir;
    std::string _interface;
    int _inputChannelCount;
    // The specific channels on the device, in case there are more than two
    // (mixers). Always doing mono for now.
    // Some mixers (as the Denon Prime X1800) presents all cheir channels
    // (10 in that case, numbered 0-9) as one device. That’s convenient to
    // record samples of individual channels of the mixer, but we may care
    // about a specific pair ... There is no channel description that makes
    // sense in Alsa as far as I can tell (the x1800 gets recognized as 7.1
    // + 2 aux channels).
    std::array<int, _outputChannelCount> _channels;
    Pcm _in;
    int _storageBytes = -1;
    FlacPtr _enc;
    std::vector<uint8_t> _readBuf;
    // flac always take int32_t i.e. signed 32 bit values
    std::vector<int32_t> _convBuf;
    size_t _readErrors = 0;
    // ---------------------------------------------------------------------- //

    std::chrono::system_clock::time_point _last = {};
    bool _buttonOn = true;
  };
}
