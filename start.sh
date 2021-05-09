#!/bin/bash

set -e

/home/pi/pisample/pisample \
  --config /home/pi/pisample/config.ini \
  |& tee pisample.logs

# read -n1 -p "coin"
