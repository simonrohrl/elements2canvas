SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Optional: duration in seconds (if provided, run in background and auto-kill)
DURATION=${1:-}

# Kill any existing Chromium instances
pkill -f "Chromium.app" 2>/dev/null || true

# Use a fresh temporary profile (empty cache, no extensions, no history)
TEMP_PROFILE=$(mktemp -d)
echo "Using temp profile: $TEMP_PROFILE"

if [ -n "$DURATION" ]; then
    # Automated mode: run in background, wait, then kill
    echo "Running Chromium for $DURATION seconds..."
    ./chromium/src/out/Default/Chromium.app/Contents/MacOS/Chromium \
        --user-data-dir="$TEMP_PROFILE" \
        http://localhost:8000/skia_paint/reference.html 2> output &
    CHROMIUM_PID=$!
    sleep "$DURATION"
    echo "Stopping Chromium..."
    pkill -f "Chromium.app" 2>/dev/null || true
    wait $CHROMIUM_PID 2>/dev/null || true
else
    # Interactive mode: wait for user to close browser
    ./chromium/src/out/Default/Chromium.app/Contents/MacOS/Chromium \
        --user-data-dir="$TEMP_PROFILE" \
        http://localhost:8000/skia_paint/reference.html 2> output
fi

# Copy debug log to project directory before cleanup
if [ -f "$TEMP_PROFILE/chrome_debug.log" ]; then
    cp "$TEMP_PROFILE/chrome_debug.log" "$PROJECT_DIR/chrome_debug.log"
    echo "Copied chrome_debug.log to $PROJECT_DIR"
else
    echo "Warning: chrome_debug.log not found in temp profile"
    # Check common alternative locations
    if [ -f ~/Library/Application\ Support/Chromium/chrome_debug.log ]; then
        cp ~/Library/Application\ Support/Chromium/chrome_debug.log "$PROJECT_DIR/chrome_debug.log"
        echo "Copied chrome_debug.log from default Chromium profile"
    fi
fi

# Cleanup temp profile
rm -rf "$TEMP_PROFILE"