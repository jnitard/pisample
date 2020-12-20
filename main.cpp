#include <iostream>
#include <utility>
#include <memory>

#include <alsa/asoundlib.h>

#include <wx/wx.h>

using namespace std;

int main(int argc, char** argv)
{
  snd_seq_t* seq = nullptr;

  int err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
  if (err < 0) {
    cerr << "Failed to initialize ALSA (open sequencer)\n";
    return EXIT_FAILURE;
  }

	err = snd_seq_set_client_name(seq, "pisample");
  if (err < 0) {
    cerr << "Failed to initialize ALSA (set client name)\n";
    return EXIT_FAILURE;
  }

	err = snd_seq_create_simple_port(seq, "pisample",
       SND_SEQ_PORT_CAP_WRITE |
       SND_SEQ_PORT_CAP_SUBS_WRITE,
       SND_SEQ_PORT_TYPE_MIDI_GENERIC |
       SND_SEQ_PORT_TYPE_APPLICATION);
  if (err < 0) {
    cerr << "Failed to initialize ALSA (create simple port)\n";
    return EXIT_FAILURE;
  }

  const char* inputName = "24";

  snd_seq_addr_t in;
  err = snd_seq_parse_address(seq, &in, inputName);
  if (err < 0) {
    cerr << "Failed to understand meaning of '" << inputName << "'\n";
    return EXIT_FAILURE;
  }

  err = snd_seq_connect_from(seq, 0, in.client, in.port);
  if (err < 0) {
    cerr << "Failed to connect to input '" << inputName << "'\n";
    return EXIT_FAILURE;
  }

  int npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
  unique_ptr<pollfd[]> pfds = make_unique<pollfd[]>(npfds);

  snd_seq_event* event = nullptr;

  while (true) {
    snd_seq_poll_descriptors(seq, pfds.get(), npfds, POLLIN);
    if (poll(pfds.get(), npfds, -1) < 0) {
      cerr << "Poll failed, quitting\n";
      return EXIT_FAILURE;
    }

    while (true) {
      err = snd_seq_event_input(seq, &event);
      if (err < 0) {
        cerr << "Failed to get next event in poll, returning\n";
        return EXIT_FAILURE;
      }
      if (event != nullptr) {
        cout << "Source: " << (int) event->source.client << ":"
             << (int) event->source.port << "\n";
      }
    }
  }

  return EXIT_SUCCESS;
}

