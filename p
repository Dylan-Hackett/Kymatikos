#!/bin/sh
# One-letter helper: silent build and flash to QSPI from anywhere
cd /Users/dylanhackett/Documents/Kymatikos || exit 1
set -e
make -s
make -s program-app
