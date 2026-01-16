#!/bin/bash
set -e

# 1. Setup depot_tools
if [ ! -d "depot_tools" ]; then
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
fi
export PATH="$(pwd)/depot_tools:$PATH"

# 2. Fetch Chromium
mkdir -p chromium
cd chromium

if [ ! -d "src" ]; then
    caffeinate fetch --no-history chromium
fi

cd src

gn gen out/Default
cp ../../scripts/assets/args.gn out/Default/args.gn