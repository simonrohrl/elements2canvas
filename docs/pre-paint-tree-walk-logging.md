# Pre-Paint Tree Walk DOM Logging

This patch adds DOM tree logging during the pre-paint phase of Chromium's rendering pipeline.

## Changes

### 1. Add required includes
**File:** `third_party/blink/renderer/core/paint/pre_paint_tree_walk.cc`

```cpp
#include "base/command_line.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
```

### 2. Add DOM tree logging in `WalkTree()`
**File:** `third_party/blink/renderer/core/paint/pre_paint_tree_walk.cc`

```cpp
Document* document = root_frame_view.GetFrame().GetDocument();
if (!document) {
  return;
}

if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("type") ==
        "renderer" &&
    document->IsInMainFrame()) {
  const int64_t timestamp_ms =
      base::TimeTicks::Now().since_origin().InMilliseconds();
  LOG(INFO) << "PrePaint URL: " << document->Url().GetString().Utf8()
            << " State: " << document->Lifecycle().GetState()
            << " timestamp (ms): " << timestamp_ms;
  ShowTree(document);
}
```

## What It Does

During each pre-paint tree walk (which happens before every paint), this logs:
- The document URL
- The lifecycle state (should be `kInPrePaint`)
- A millisecond timestamp
- The full DOM tree via `ShowTree(document)`

## Requirements

This patch requires the `node-tree-debug.patch` to be applied first, as it depends on `ShowTree()` being available in release builds.

## Filtering

The logging only triggers for:
- Renderer processes (checked via `--type=renderer` command line flag)
- Main frame documents (not iframes)

This prevents excessive logging from subframes and non-renderer processes.
