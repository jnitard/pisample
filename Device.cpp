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

  snd_seq_event initDev;
  memset(&initDev, 0, sizeof(initDev));
  snd_seq_ev_set_direct(&initDev);
  initDev.type = SND_SEQ_EVENT_NOTEOFF;
  initDev.source.port = _hostPort;
  initDev.dest = _deviceAddress;
  initDev.data.note.channel = 0xf;
  initDev.data.note.note = 0x00;
  initDev.data.note.velocity = 0x7f;
  err = snd_seq_event_output_direct(_seq, &initDev);
  if (err < 0) {
    throw DeviceInitError("Failed to send initial setup to device: {}", err);
  }

  _nfds = snd_seq_poll_descriptors_count(_seq, POLLIN);
  _fds = make_unique<pollfd[]>(_nfds);

  // TODO: drain input queue before doing anything - that will avoid widly unrelated
  // key presses

  // TODO: pollin only ?

  // TODO: may need to be moved inside the loop in case we change it
  // dynamically ?
  snd_seq_poll_descriptors(_seq, _fds.get(), _nfds, POLLIN);
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

    cout << "Source: " << (int) event->source.client << ":"
          << (int) event->source.port
          << ", type: " << alsa::eventToString(event->type);
    switch (event->type) {
      case SND_SEQ_EVENT_NOTEON:
      case SND_SEQ_EVENT_NOTEOFF:
        cout << ", channel: " << (int)event->data.note.channel
              << ", note: " << (int)event->data.note.note
              << ", velocity: " << (int)event->data.note.velocity;
        break;
      case SND_SEQ_EVENT_CHANPRESS:
      case SND_SEQ_EVENT_CONTROLLER:
        cout << ", channel: " << (int)event->data.control.channel
              << ", param: " << event->data.control.param
              << ", value: " << event->data.control.value;
        break;
      case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
        cerr << ", Device disconnected, exiting\n";
        return EXIT_FAILURE;
    }
    cout << "\n";
  } while (snd_seq_event_input_pending(_seq, false) > 0);

  return true;
}