# Implementation Plan: Fix Scripts to Match Chromium Behavior

## Requirements
- Output format must match reference files **exactly** (same field names, structure)
- `is_self_painting` field needs to be added to Chromium logging (input data)

---

## Key Chromium Source Files

1. **paint_layer.cc** - `InsertOnlyThisLayerAfterStyleChange()` (lines 829-852), `UpdateStackingNode()` (lines 858-877)
2. **paint_layer_stacking_node.cc** - `CollectLayers()` (lines 315-338), `RebuildZOrderLists()` (lines 282-313)
3. **paint_layer_painter.cc** - Logging code (lines 541-594)

---

## Identified Issues

### Issue 1: Z-Order Sort Stability
JavaScript's `Array.sort()` is not guaranteed stable. Chromium uses `std::stable_sort`.

### Issue 2: Output Format Differences
| Computed | Reference |
|----------|-----------|
| `id` | `layer_id` |
| `stacking_context` | `layer` |
| `needs_stacking_node` | `has_stacking_node` |
| Includes all 49 stacking contexts | Only 7 with stacking nodes |

### Issue 3: Missing `is_self_painting` Field
Reference `layer_tree.json` has `is_self_painting`, computed output lacks it.

### Issue 4: Different `depth` Representation
Reference uses single `depth` field, computed uses `layer_depth` and `layout_depth`.

---

## Implementation Steps

### Step 1: Update Chromium Logging (`paint_layer_painter.cc`)

Add `is_self_painting` to the LayoutObject serialization:

**File:** `chromium/src/third_party/blink/renderer/core/paint/paint_layer_painter.cc`

In `SerializeLayoutObject()` (around line 177-194), add:
```cpp
if (object.HasLayer()) {
  json->SetBoolean("is_self_painting",
    To<LayoutBoxModelObject>(object).Layer()->IsSelfPaintingLayer());
}
```

### Step 2: Fix `compute_layer_tree.js`

1. **Output format changes to match `layer_tree.json`:**
   - Use `depth` instead of `layer_depth`
   - Remove `layout_depth` field
   - Remove `tag` field
   - Remove `summary` wrapper - output just the array
   - Add `is_self_painting` field (from input data)
   - Ensure `parent_id: null` for root (not omitted)

### Step 3: Fix `compute_stacking_nodes.js`

1. **Use stable sort** for z-order lists:
   ```javascript
   const sorted = stackedDescendants
     .map((d, i) => ({...d, _order: i}))
     .filter(d => d.z_index >= 0)
     .sort((a, b) => a.z_index - b.z_index || a._order - b._order);
   ```

2. **Output format changes to match `stacking_nodes.json`:**
   - Use `layer_id` instead of `id`
   - Use `layer` instead of `stacking_context`
   - Use `has_stacking_node` instead of `needs_stacking_node`
   - Only output stacking contexts that have stacking nodes
   - Remove extra fields: `layer_depth`, `layout_depth`, `z_index`, `has_stacked_descendant`
   - Remove `is_stacking_context` from z-order list items
   - Remove `summary` wrapper

---

## Files to Modify

1. `chromium/src/third_party/blink/renderer/core/paint/paint_layer_painter.cc`
2. `compute_layer_tree.js`
3. `compute_stacking_nodes.js`

---

## Verification

1. Rebuild Chromium and regenerate input data
2. Run `node compute_layer_tree.js`
3. Run `node compute_stacking_nodes.js`
4. `diff` outputs against reference files - should show no differences

---

## Status: COMPLETED

All changes have been implemented and verified:

```bash
$ diff <(jq -S . computed_layer_tree.json) <(jq -S . layer_tree.json)
# No output - files match!

$ diff <(jq -S . computed_stacking_nodes.json) <(jq -S . stacking_nodes.json)
# No output - files match!
```

### Changes Made:

1. **paint_layer_painter.cc** - Added `is_self_painting` and `computed_style` to LayoutObject serialization
   - `is_self_painting`: Whether the layer is self-painting
   - `computed_style`: Full ComputedStyle object with:
     - `display`, `position`, `visibility`
     - `opacity`, `z_index`
     - `has_transform`, `has_3d_transform`
     - `overflow_x`, `overflow_y`
     - `has_filter`, `has_backdrop_filter`, `has_clip_path`, `has_mask`
     - `has_blend_mode`, `is_isolated`
     - `has_will_change_transform`, `has_will_change_opacity`
     - `contain_paint`, `contain_layout`
     - `background_color`, `color`
2. **compute_layer_tree.js** - Updated output format to match `layer_tree.json`:
   - Uses `depth` instead of `layer_depth`
   - Removed `tag`, `layout_depth`, summary wrapper
   - Added `is_self_painting` field
3. **compute_stacking_nodes.js** - Updated stable sort and output format:
   - Implemented stable sort using insertion order as tiebreaker
   - Uses `layer_id`, `layer`, `has_stacking_node` field names
   - Only outputs stacking contexts with stacking nodes (7 items)
   - Removed extra fields from z-order list items
