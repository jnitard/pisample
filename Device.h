#pragma once

/// \file Talk to an Atom device - with Alsa APIs.
/// Not trying to support anything else

#include "Atom.h"
#include "Synth.h"
#include "fmt.h"

#include <alsa/asoundlib.h>

#include <memory>
#include <exception>

namespace ps
{
  /// Fill-in an ALSA struct from our internal representation.
  void convertNote(atom::Note, snd_seq_ev_note& out);

  /// Helper for erroring out fast.
  class DeviceInitError : public FormattedException
  {
  public:
    using FormattedException::FormattedException;
  };

  /// Something went wrong while polling or sending.
  class DeviceFailure : public FormattedException
  {
  public:
    using FormattedException::FormattedException;
  };

  void convertNote(atom::Note note, snd_seq_event& out);

  /// Talk to an Atom device, gives callbacks to a synth if given
  class Device
  {
  public:
    Device(const char* devicePortName);
    Device(const Device&) = delete;
    ~Device();

    void setSynth(Synth& synth) { _synth = & synth; }

    /// Returns true if anything changed, potentially indicating more things
    /// to poll.
    bool poll();

    void sendNotes(const std::vector<atom::Note>& notes);
    void sendControl(atom::Control);

  private:
    void setCustomMode();
    void step();

    snd_seq_t* _seq;
    int _hostPort;
    snd_seq_addr_t _deviceAddress;
    std::unique_ptr<pollfd[]> _fds;
    int _nfds;

    Synth* _synth = nullptr;
  };
}