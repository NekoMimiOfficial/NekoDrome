#!/usr/bin/bash
set -e

make -j 12
./deploy.sh
