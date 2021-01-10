#include "Alsa.h"

#include <iostream>

using namespace ps;
using namespace std;

Pcm::~Pcm()
{
  snd_pcm_drop(Ptr);
  snd_pcm_close(Ptr);
}

Pcm::Pcm(std::string_view interface,
         snd_pcm_stream_t direction,
         int channelCount,
         std::array<int, 2> channels)
try {
  Ptr = nullptr;
  int err = snd_pcm_open(&Ptr, string(interface).c_str(), direction, 0);
  if (err < 0) {
    throw Exception("Could not open recording device {} : {}",
        interface, AlsaErr{err});
  }

  if (channelCount == -1) {
    // Input channel count was not given on the command line, try to guess it
    // from the channel maps. TODO: there are probably better ways.
    auto chmaps_init = snd_pcm_query_chmaps(Ptr);
    auto chmaps = chmaps_init;
    while (chmaps != nullptr and *chmaps != nullptr) {
      // char buf[2048];
      // snd_pcm_chmap_print(&(*chmaps)->map, 2048, buf);
      // std::cout << buf;
      channelCount = (*chmaps)->map.channels;
      if (channelCount > max(channels[0], channels[1]) ) {
        break;
      }
      channelCount = -1; // not enough channels don’t remember the value
      ++chmaps;
    }
    snd_pcm_free_chmaps(chmaps_init);
  }

  if (channelCount == -1) {
    throw Exception("Could not guess the total number of input channels. "
      "This is likely when working with pulse. You can give "
      "--audio-{}-channel-count X to set the value.",
      direction == SND_PCM_STREAM_PLAYBACK ? "out" : "in");
  }

  if (any_of(begin(channels), end(channels),
       [&](int c) { return c >= channelCount; }))
  {
    throw Exception("All channels given to --audio-{}-channels must be "
        "less than the total number of channels ({})",
        direction == SND_PCM_STREAM_PLAYBACK ? "out" : "in", channelCount);
  }

  // The order is as the comment that we want the highest quality first.
  // TODO:
  // On my setup is so happens that the highest quality works. snd_pcm_set_params
  // will print errors when things don’t work unfortunately :( so we would ideally
  // need to use the lower level APIs to test things out.
  for (int rate : {48000, 44100}) {
    for (snd_pcm_format_t format:
         {SND_PCM_FORMAT_S32_LE, SND_PCM_FORMAT_S24_LE, SND_PCM_FORMAT_S16_LE })
    {
      int err = snd_pcm_set_params(
        Ptr,
        format,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        channelCount,
        rate,
        0 /* no soft re-sample */,
        0 /* micro secs, latency */); // TODO: not sure how this works
      if (err >= 0) {
        Format.Rate = rate;
        Format.Bits = formatBits(format);
        Format.Channels = channelCount;
        return;
      }
    }
  }

  throw Exception("Device {} does not support any format we support (44100 or "
      "48000 sampling rate, *signed* 16, 24 or 32 bit little endian).",
      interface);
}
catch (...) {
  if (Ptr) {
    snd_pcm_close(Ptr);
  }
}


const char* ps::eventToString(snd_seq_event_type_t event)
{
  #define ALSA_CASE(x) case SND_SEQ_EVENT_ ## x: return #x;
  switch ((snd_seq_event_type)event) {
    ALSA_CASE(SYSTEM);
    ALSA_CASE(RESULT);
    ALSA_CASE(NOTE);
    ALSA_CASE(NOTEON);
    ALSA_CASE(NOTEOFF);
    ALSA_CASE(KEYPRESS);
    ALSA_CASE(CONTROLLER);
    ALSA_CASE(PGMCHANGE);
    ALSA_CASE(CHANPRESS);
    ALSA_CASE(PITCHBEND);
    ALSA_CASE(CONTROL14);
    ALSA_CASE(NONREGPARAM);
    ALSA_CASE(REGPARAM);
    ALSA_CASE(SONGPOS);
    ALSA_CASE(SONGSEL);
    ALSA_CASE(QFRAME);
    ALSA_CASE(TIMESIGN);
    ALSA_CASE(KEYSIGN);
    ALSA_CASE(START);
    ALSA_CASE(CONTINUE);
    ALSA_CASE(STOP);
    ALSA_CASE(SETPOS_TICK);
    ALSA_CASE(SETPOS_TIME);
    ALSA_CASE(TEMPO);
    ALSA_CASE(CLOCK);
    ALSA_CASE(TICK);
    ALSA_CASE(QUEUE_SKEW);
    ALSA_CASE(SYNC_POS);
    ALSA_CASE(TUNE_REQUEST);
    ALSA_CASE(RESET);
    ALSA_CASE(SENSING);
    ALSA_CASE(ECHO);
    ALSA_CASE(OSS);
    ALSA_CASE(CLIENT_START);
    ALSA_CASE(CLIENT_EXIT);
    ALSA_CASE(CLIENT_CHANGE);
    ALSA_CASE(PORT_START);
    ALSA_CASE(PORT_EXIT);
    ALSA_CASE(PORT_CHANGE);
    ALSA_CASE(PORT_SUBSCRIBED);
    ALSA_CASE(PORT_UNSUBSCRIBED);
    ALSA_CASE(USR0);
    ALSA_CASE(USR1);
    ALSA_CASE(USR2);
    ALSA_CASE(USR3);
    ALSA_CASE(USR4);
    ALSA_CASE(USR5);
    ALSA_CASE(USR6);
    ALSA_CASE(USR7);
    ALSA_CASE(USR8);
    ALSA_CASE(USR9);
    ALSA_CASE(SYSEX);
    ALSA_CASE(BOUNCE);
    ALSA_CASE(USR_VAR0);
    ALSA_CASE(USR_VAR1);
    ALSA_CASE(USR_VAR2);
    ALSA_CASE(USR_VAR3);
    ALSA_CASE(USR_VAR4);
    ALSA_CASE(NONE);
  }
  #undef ALSA_CASE
  return "Unknown Alsa Event";
}