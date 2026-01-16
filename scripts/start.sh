SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXTENSION_PATH="$SCRIPT_DIR/../extension"

../chromium/src/out/Default/Chromium.app/Contents/MacOS/Chromium \
  --load-extension="$EXTENSION_PATH" \
  ../eval/sample.html 2> output