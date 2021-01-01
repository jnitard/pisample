#include "Device.h"
#include "Alsa.h"

#include <iostream>
#include <thread>
#include <chrono>

#include <alsa/asoundlib.h>

using namespace atom;
using namespace std;
using namespace ps;

namespace c = std::chrono;

namespace
{
  bool toOnOff(snd_seq_event_type_t type)
  {
    return type == SND_SEQ_EVENT_NOTEON;
  }
}

void ps::convertNote(atom::Note note, snd_seq_event& out)
{
  out.type = note.OnOff ? SND_SEQ_EVENT_NOTEON : SND_SEQ_EVENT_NOTEOFF;
  out.data.note.channel = note.Channel;
  out.data.note.note = static_cast<uint8_t>(note.Note);
  out.data.note.velocity = note.Velocity;
}

Device::Device(const char* devicePortName)
{
  int err = snd_seq_open(&_seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
  if (err < 0) {
    throw DeviceInitError("Failed to initialize ALSA (open sequencer)");
  }

	err = snd_seq_set_client_name(_seq, "pisample");
  if (err < 0) {
    throw DeviceInitError("Failed to initialize ALSA (set client name)");
  }

	err = snd_seq_create_simple_port(_seq, "host-atom",
       SND_SEQ_PORT_CAP_READ |
       SND_SEQ_PORT_CAP_SUBS_READ |
       SND_SEQ_PORT_CAP_WRITE |
       SND_SEQ_PORT_CAP_SUBS_WRITE,
       SND_SEQ_PORT_TYPE_MIDI_GENERIC |
       SND_SEQ_PORT_TYPE_APPLICATION);
  if (err < 0) {
    throw DeviceInitError("Failed to initialize ALSA (create simple port)");
  }

  _hostPort = err;
  cerr << "Our port: [app]: " << _hostPort << '\n';

  err = snd_seq_parse_address(_seq, &_deviceAddress, devicePortName);
  if (err < 0) {
    throw DeviceInitError("Failed to understand meaning of '{}'", devicePortName);
  }

  err = snd_seq_connect_from(_seq, _hostPort,
      _deviceAddress.client, _deviceAddress.port);
  if (err < 0) {
    throw DeviceInitError("Failed to connect to input '{}'", devicePortName);
  }

  err = snd_seq_connect_to(_seq, _hostPort,
      _deviceAddress.client, _deviceAddress.port);
  if (err < 0) {
    throw DeviceInitError("Failed to connect to output '{},", devicePortName);
  }
  cerr << "Connected to device port: "
       << (int)_deviceAddress.client << ":" << (int)_deviceAddress.port << "\n";


  sendNotes(atom::initSequence());

  forAllButtons([this](Buttons b) { sendControl(Control{ (uint8_t)b, 0x00 }); });

  _nfds = snd_seq_poll_descriptors_count(_seq, POLLIN);
  _fds = make_unique<pollfd[]>(_nfds);

  // TODO: may need to be moved inside the loop in case we change it
  // dynamically ?
  snd_seq_poll_descriptors(_seq, _fds.get(), _nfds, POLLIN);

  // Drain any input sent before we started.
  while (true) {
    int nActive = ::poll(_fds.get(), _nfds, 0);
    if (nActive > 0) {
      snd_seq_event_t* event = nullptr;
      snd_seq_event_input(_seq, &event);
    }
    else {
      break;
    }
  }
}

Device::~Device()
{
  snd_seq_close(_seq);
}

void Device::sendNotes(const vector<Note>& notes)
{
  snd_seq_event note;

  for (auto& n: notes) {
    memset(&note, 0, sizeof(note));
    snd_seq_ev_set_direct(&note);
    note.type = n.OnOff ? SND_SEQ_EVENT_NOTEON : SND_SEQ_EVENT_NOTEOFF;
    note.source.port = _hostPort;
    note.dest = _deviceAddress;
    note.data.note.channel = n.Channel;
    note.data.note.note = n.Note;
    note.data.note.velocity = n.Velocity;

    int err = snd_seq_event_output_direct(_seq, &note);
    if (err < 0) {
      throw DeviceInitError("Failed to send note to device: {}", err);
    }
  }
}

void Device::sendControl(Control c)
{
  snd_seq_event ctl;
  memset(&ctl, 0, sizeof(ctl));
  snd_seq_ev_set_direct(&ctl);
  ctl.type = SND_SEQ_EVENT_CONTROLLER;
  ctl.source.port = _hostPort;
  ctl.dest = _deviceAddress;
  ctl.data.control.channel = 0;
  ctl.data.control.param = c.Param;
  ctl.data.control.value = c.Value;

  int err = snd_seq_event_output_direct(_seq, &ctl);
  if (err < 0) {
    throw DeviceInitError("Failed to send control to device: {}", err);
  }
}

bool Device::poll()
{
  int nActive = ::poll(_fds.get(), _nfds, 0);
  if (nActive == 0) {
    return false;
  }

  if (nActive < 0) {
    throw DeviceFailure("Failed to get next event in poll");
  }

  snd_seq_event_t* event = nullptr;
  do {
    // Note : should always have something since poll succeeded.
    // We are in blocking mode though blocking is not wished.
    int err = snd_seq_event_input(_seq, &event);
    if (err < 0) {
      throw DeviceFailure("snd_seq_event_input had an error");
    }
    if (event == nullptr) {
      break; // should have happened really, but defensive
    }

    auto printType = [&] {
      cout << "Source: " << (int) event->source.client << ":"
           << (int) event->source.port
           << ", type: " << eventToString(event->type);
    };

    switch (event->type) {
      case SND_SEQ_EVENT_NOTEON:
      case SND_SEQ_EVENT_NOTEOFF:
        if (_synth) {
          _synth->event(Note{
            .OnOff = toOnOff(event->type),
            .Channel = event->data.note.channel,
            .Note = event->data.note.note,
            .Velocity = event->data.note.velocity
          });
        }
        else {
          printType();
          cout << '\n';
        }
        break;

      case SND_SEQ_EVENT_CONTROLLER:
        if (event->data.control.channel != 0) {
          cerr << "UNKNOWN CHANNEL IN CONTROL: " <<
            (int)event->data.control.channel << ", dropping\n";
          break;
        }

        if (_synth) {
          _synth->event(Control{
            .Param = (uint8_t)event->data.control.param,
            .Value = (uint8_t)event->data.control.value
          });
        }
        else {
          printType();
          cout << '\n';
        }

        break;

      case SND_SEQ_EVENT_CHANPRESS:
        printType();
        cout << '\n';
        break;
      case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
        printType();
        cerr << ", Device disconnected, exiting\n";
        return EXIT_FAILURE;
    }
  } while (snd_seq_event_input_pending(_seq, false) > 0);

  return true;
}
