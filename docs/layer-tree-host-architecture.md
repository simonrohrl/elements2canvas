# LayerTreeHost vs LayerTreeHostImpl

## Overview

Chromium's compositor uses a **two-thread architecture** to keep the UI responsive:

| Thread | Owner | Purpose |
|--------|-------|---------|
| Main thread | `LayerTreeHost` | Blink runs here (JS, layout, paint) |
| Compositor thread | `LayerTreeHostImpl` | Scrolling, animations, rasterization, drawing |

## Data Flow

```
Main Thread                      Compositor Thread
───────────                      ─────────────────
LayerTreeHost                    LayerTreeHostImpl
    │                                   │
    ├── cc::Layer                      ├── pending_tree (LayerTreeImpl)
    ├── PropertyTrees                  │       └── cc::LayerImpl
    │                                  │       └── PropertyTrees (copy)
    │                                  │
    └────────── COMMIT ───────────────►├── active_tree (LayerTreeImpl)
                                       │       └── cc::LayerImpl
                                       │       └── PropertyTrees
                                       │
                                       └──────► Draw
```

## The Commit Process

1. **BeginCommit**: Creates pending_tree if needed
2. **FinishCommit**: Copies data from main thread
   - `TreeSynchronizer` creates `LayerImpl` for each `Layer`
   - `Layer::PushPropertiesTo(LayerImpl*)` copies layer properties
   - `PropertyTrees` are copied
3. **CommitComplete**: Updates draw properties
4. **ActivateSyncTree**: Pushes pending_tree → active_tree

## Two Trees on Impl Side

`LayerTreeHostImpl` maintains two `LayerTreeImpl` instances:

- **pending_tree**: Receives new commits, may be rasterizing tiles
- **active_tree**: Currently being drawn to screen

This allows the compositor to keep drawing the old frame while preparing the new one.

## Key Files

- `cc/trees/layer_tree_host.h` - Main thread interface
- `cc/trees/layer_tree_host_impl.h` - Compositor thread implementation
- `cc/trees/layer_tree_impl.h` - Tree structure (used by pending/active trees)
- `cc/trees/tree_synchronizer.cc` - Synchronizes Layer → LayerImpl
- `cc/layers/layer.h` - Main thread layer
- `cc/layers/layer_impl.h` - Compositor thread layer

## Why This Architecture?

Decoupling allows:
- Smooth 60fps scrolling even when JS is busy
- Compositor-driven animations without main thread involvement
- Tile-based rasterization in parallel with main thread work
