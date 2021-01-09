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
  class Recorder
  {
  public:
    Recorder(Device& d, const ArgMap&);
    ~Recorder();

    static ArgMap args();

    /// start or stop recording, when the record button is pressed
    void toggle();

    void poll();

  private:
    Device& _device;
    std::atomic<bool> _on   = false;
    std::atomic<bool> _stop = false;

    // Try some easy to implement format first.
    // If a format is not supported I’d recommend switching input to pulse
    // at much more CPU expense.
    void findCompatibleFormat();

    void startRecording();
    void stopRecording();
    void recordFrames();
    void run();

    std::thread _thread;

    int _sampleBits = -1;
    int _storageBytes = -1;
    snd_pcm_format_t _format;
    int _inputChannelCount;
    static constexpr int _outputChannelCount = 2;
    // when nothing specified just take the first two.
    // TODO: handle mono.
    std::array<int, _outputChannelCount> _channels;
    
    // **** these variables should only be accessed in the rec thread ****
    // (or in the constructor)
    int _rate = -1;
    // we don’t need 10 channels, we need channels 8 and 9 :(
    // though not sure how to get just those two with alsa
    PcmPtr _in;
    FlacPtr _enc;
    std::vector<uint8_t> _readBuf;
    // flac always take int32_t i.e. signed 32 bit values
    std::vector<int32_t> _convBuf;
    size_t _readErrors = 0;
    std::string _interface;
    // **** --------------------------------------------------------- ****

    std::chrono::system_clock::time_point _last = {};
    bool _buttonOn = true;
  };
}
