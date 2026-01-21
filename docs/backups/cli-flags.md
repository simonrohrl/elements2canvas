# CLI Flags Patch

This patch modifies `chrome/app/chrome_main_delegate.cc` to automatically append default command-line flags, eliminating the need to pass them manually when launching Chromium.

## Flags Applied

### `--run-all-compositor-stages-before-draw`
Forces synchronous execution of all compositor stages before drawing. This ensures deterministic rendering behavior, which is essential for consistent DOM tracing. This especially ensures that the compositor always waits for the main thread before drawing.

### `--enable-logging`
Enables Chromium's logging infrastructure. Required to capture debug output and trace information.

### `--v=0`
Sets the verbosity level to 0 (default). This can be overridden by explicitly passing a different `--v` value on the command line.

### `--use-mock-keychain` (macOS only)
Uses a mock keychain instead of the system keychain. This avoids keychain access prompts during automated runs on macOS.

### `--disable-features=DialMediaRouteProvider`
Avoids permission dialog after every build.

## Behavior

Each flag is only appended if not already present on the command line, allowing users to override defaults when needed. For `--disable-features`, the patch appends `DialMediaRouteProvider` to any existing disabled features rather than replacing them.
