#pragma once

#include "Alsa.h"
#include "Arguments.h"
#include "PadsAccess.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>


namespace ps
{

  class Player : public PadsAccess
  {
  public:
    static ArgMap args();

    Player(const ArgMap& args, Pads& pads);
    Player(const Player&) = delete;
    ~Player();

    /// Add a new sample, returns the index used to replay it.
    /// NOTE: loading a new sample will not happen while anything is playing.
    /// 0-based return value, i.e. 0 is a valid sample index.
    int load(FrameFormat, std::vector<uint8_t>&& bytes);
    /// Same as above but reads the file from the disk for you as well.
    int load(std::filesystem::path);

    /// Start playing the given sample index as returned per load.
    /// Other samples will keep playing.
    void play(int);

    /// -1 to stop everything
    void stop(int);

  private:
    void run();

    std::string _interface;
    std::thread _thread;
    std::atomic<bool> _stop = 0;

    // This tells the playing thread to check for _nextSamples when it is
    // mismatching the size of _samples. This is used to avoid locking for
    // long when adding new samples. We can return the sample index
    // immediately (yes, I like overkill optimizations).
    std::atomic<int> _count = 0;

    std::mutex _mutex;
    // ---------- members below must be accessed under the mutex ------------ //
    std::vector<std::string> _nextFiles;
    std::vector<std::vector<uint8_t>> _nextSamples;
    // ---------------------------------------------------------------------- //

    // --------- members below must be accessed in the thread only ---------- //
    int _outputChannelCount;
    std::array<int, 2> _channels; // the channels to playback on.
    Pcm _out;

    std::vector<std::vector<uint8_t>> _samples;

    // ---------------------------------------------------------------------- //

  };
}