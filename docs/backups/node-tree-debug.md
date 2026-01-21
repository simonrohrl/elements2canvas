# DOM Node Tree Debug Functions in Release Builds

This patch makes DOM tree debugging functions available in release builds, not just debug builds.

## Changes

### 1. Move `ToTreeStringForThis()` out of DCHECK guard
**File:** `third_party/blink/renderer/core/dom/node.h` and `node.cc`

The method is moved outside `#if DCHECK_IS_ON()` so it's available in release builds.

### 2. Move `ToMarkedTreeString()` out of DCHECK guard
**File:** `third_party/blink/renderer/core/dom/node.h` and `node.cc`

Along with its helper `AppendMarkedTree()`, this is now available in release builds.

### 3. Move `ShowTree()` out of DCHECK guard
**File:** `third_party/blink/renderer/core/dom/node.cc`

```cpp
void ShowTree(const blink::Node* node) {
  if (node)
    LOG(INFO) << "\n" << node->ToTreeStringForThis().Utf8();
  else
    LOG(INFO) << "Cannot showTree for <null>";
}
```

This global function can now be called from release builds to dump the DOM tree.

## Why This Works

In standard Chromium builds, these debugging functions are only compiled when `DCHECK_IS_ON()` (debug/dcheck builds). This patch selectively moves the tree-printing functions outside the guard while keeping the flat-tree variants debug-only.

This allows calling `ShowTree(node)` from a debugger or adding logging statements to trace DOM structure in release builds.
