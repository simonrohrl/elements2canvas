SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXTENSION_PATH="$SCRIPT_DIR/../extension"

# Kill any existing Chromium instances
pkill -f "Chromium.app" 2>/dev/null || true

rm -rf ~/Library/Application\ Support/Chromium/chrome_debug.log
./chromium/src/out/Default/Chromium.app/Contents/MacOS/Chromium http://localhost:8000/eval/sample.html 2> output