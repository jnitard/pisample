#!/bin/bash

/home/pi/pisample/pisample \
    --midi-in-port "ATOM MIDI 1" \
    --audio-in-card "dsnoop:CARD=Prime" \
    --audio-in-channels 8,9 \
    --audio-out-card "dmix:CARD=Prime" \
    --audio-out-channels 6,7 \
    --audio-out-channel-count 10 \
  |& tee pisample.logs

# read -n1 -p "coin"
