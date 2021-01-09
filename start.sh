#!/bin/bash

/home/pi/pisample/pisample \
    --midi-in-port "ATOM MIDI 1" \
    --audio-in-card "dsnoop:CARD=Prime" \
    --audio-in-channels 8,9 \
  |& tee pisample.logs

# read -n1 -p "coin"
