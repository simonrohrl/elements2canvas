SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXTENSION_PATH="$SCRIPT_DIR/../extension"
rm -rf ~/Library/Application\ Support/Chromium/chrome_debug.log
./chromium/src/out/Default/Chromium.app/Contents/MacOS/Chromium --start-fullscreen ./eval/sample.html 2> output