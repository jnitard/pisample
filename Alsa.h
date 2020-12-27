#pragma once

/// \file Some helpers for ALSA.

#include <alsa/seq_event.h>

namespace alsa
{
  /// Turn an event into string, mostly useful for debugging.
  constexpr const char* eventToString(snd_seq_event_type_t event)
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
    return "UNKNOWN";
  }
}