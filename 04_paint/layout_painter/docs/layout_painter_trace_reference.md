# Layout Painter Trace Reference

This document provides a detailed line-by-line trace of the key functions in the layout_painter group.

## 1. PaintLayerPainter::Paint() - Main Entry Point

**File:** `paint_layer_painter.cc`
**Lines:** 810-1153

### Function Signature
```cpp
PaintResult PaintLayerPainter::Paint(GraphicsContext& context, PaintFlags paint_flags)
```

### Parameters
- `context`: GraphicsContext for painting
- `paint_flags`: Optional flags (default: PaintFlag::kNoFlag)

### Line-by-Line Trace

| Lines | Function/Check | Conditional | Description |
|-------|----------------|-------------|-------------|
| 812-818 | `object.NeedsLayout()` | YES | Skip if needs layout (bug safeguard) |
| 820-822 | `GetFrameView()->ShouldThrottleRendering()` | YES | Skip if rendering throttled |
| 824-826 | `IsFragmentLessBox()` | YES | Skip fragmentless boxes |
| 828-833 | `!IsSelfPaintingLayer() && !HasSelfPaintingLayerDescendant()` | YES | Skip non-self-painting layers without descendants |
| 835-843 | Position visibility checks | YES | Skip if invisible for position visibility |
| 847-852 | `!HasLocalBorderBoxProperties()` | YES | Early return for missing properties |
| 854-944 | Logging block (IsA<LayoutView>) | YES | Log SHAPE, LAYER, STACKING, PROPERTIES at root |
| 946-950 | `selection_drag_image_only && !IsSelected()` | YES | Skip unselected for drag image |
| 952-966 | `IgnorePaintTimingScope` | ALWAYS | Handle opacity and document element visibility for LCP |
| 968-1016 | `should_paint_content` calculation | ALWAYS | Determine if layer content should paint |
| 1018-1030 | Subsequence caching check | YES | Try to use cached subsequence |
| 1032-1038 | `PaintTimelineReporter`, `ScopedEffectivelyInvisible` | CONDITIONAL | Timeline reporting and invisibility scope |
| 1040-1070 | `ScopedPaintChunkProperties` for layer | YES | Set up chunk properties if painting content |
| 1072-1076 | `PaintWithPhase(kSelfBlockBackgroundOnly)` | YES | Paint self background |
| 1078-1081 | `PaintChildren(kNegativeZOrderChildren)` | ALWAYS | Paint negative z-order children |
| 1083-1095 | Foreground painting | YES | Paint foreground with scoped properties |
| 1098-1104 | `PaintWithPhase(kSelfOutlineOnly)` | YES | Paint outline (non-video first) |
| 1106-1119 | `PaintChildren(kNormalFlowAndPositiveZOrderChildren)` | ALWAYS | Paint normal flow and positive z-order |
| 1111-1119 | `PaintOverlayOverflowControls()` | YES | Paint overlay scrollbars |
| 1123-1127 | `PaintWithPhase(kSelfOutlineOnly)` | YES | Paint video outline last |
| 1129-1149 | Paint properties handling | YES | Mask, clip-path, transitions |
| 1151-1152 | `SetPreviousPaintResult(result)` | ALWAYS | Store result for subsequence caching |

### Key Decision Points

1. **Self-painting check (lines 830-833)**
   ```cpp
   if (!paint_layer_.IsSelfPaintingLayer() && !paint_layer_.HasSelfPaintingLayerDescendant())
     return kFullyPainted;
   ```

2. **Content painting decision (lines 968-974)**
   ```cpp
   bool should_paint_content =
       paint_layer_.HasVisibleContent() &&
       !paint_layer_.IsUnderSVGHiddenContainer() && is_self_painting_layer;
   ```

3. **Cull rect intersection (lines 976-1012)**
   - Computes `cull_rect_intersects_self` and `cull_rect_intersects_contents`
   - Sets `result = kMayBeClippedByCullRect` if not fully contained

---

## 2. PaintLayerPainter::PaintChildren()

**File:** `paint_layer_painter.cc`
**Lines:** 1210-1265

### Function Signature
```cpp
PaintResult PaintLayerPainter::PaintChildren(
    PaintLayerIteration children_to_visit,
    GraphicsContext& context,
    PaintFlags paint_flags)
```

### Parameters
- `children_to_visit`: Which children to iterate (kNegativeZOrderChildren, kNormalFlowAndPositiveZOrderChildren, etc.)

### Line-by-Line Trace

| Lines | Function/Check | Conditional | Description |
|-------|----------------|-------------|-------------|
| 1214-1217 | `!HasSelfPaintingLayerDescendant()` | YES | Early return if no descendants |
| 1219-1222 | `ChildPaintBlockedByDisplayLock()` | YES | Early return if display locked |
| 1225-1227 | `IsA<HTMLCanvasElement>` | YES | Prevent canvas fallback content |
| 1229-1262 | Iterator loop | ALWAYS | Iterate children in paint order |
| 1231-1233 | `IsReplacedNormalFlowStacking()` | YES | Skip replaced normal flow stacking |
| 1235-1241 | View transition root check | YES | Skip view-transition pseudo |
| 1243-1246 | `PaintLayerPainter(*child).Paint()` | ALWAYS | RECURSIVE paint call |
| 1248-1261 | Overlay overflow controls | YES | Handle reordered overlay controls |

### Paint Order (via PaintLayerPaintOrderIterator)

The iterator returns children in this order:
1. `kNegativeZOrderChildren` - Negative z-index stacked children
2. `kNormalFlowChildren` - Non-positioned children in DOM order
3. `kPositiveZOrderChildren` - Zero and positive z-index stacked children

---

## 3. PaintLayerPainter::PaintWithPhase()

**File:** `paint_layer_painter.cc`
**Lines:** 1322-1359

### Function Signature
```cpp
void PaintLayerPainter::PaintWithPhase(
    PaintPhase phase,
    GraphicsContext& context,
    PaintFlags paint_flags)
```

### Line-by-Line Trace

| Lines | Function/Check | Conditional | Description |
|-------|----------------|-------------|-------------|
| 1325-1327 | Get `layout_box_with_fragments` | ALWAYS | Check for NG box fragments |
| 1331-1334 | `multiple_fragments_allowed` check | ALWAYS | Determine if multiple fragments can paint |
| 1336-1358 | Fragment iteration loop | ALWAYS | Iterate all fragments |
| 1338-1343 | Get `physical_fragment` | YES | Get NG fragment if available |
| 1345-1348 | `ScopedDisplayItemFragment` | YES | Scope for fragment index > 0 |
| 1350-1351 | `PaintFragmentWithPhase()` | ALWAYS | Paint the fragment |
| 1353-1355 | Multiple fragments check | YES | Break if not allowed |

---

## 4. PaintLayerPainter::PaintFragmentWithPhase()

**File:** `paint_layer_painter.cc`
**Lines:** 1275-1320

### Function Signature
```cpp
void PaintLayerPainter::PaintFragmentWithPhase(
    PaintPhase phase,
    const FragmentData& fragment_data,
    wtf_size_t fragment_data_idx,
    const PhysicalBoxFragment* physical_fragment,
    GraphicsContext& context,
    PaintFlags paint_flags)
```

### Line-by-Line Trace

| Lines | Function/Check | Conditional | Description |
|-------|----------------|-------------|-------------|
| 1282-1283 | Assert self-painting or overlay | ALWAYS | DCHECK validation |
| 1285-1288 | Cull rect empty check | YES | Early return if empty cull rect |
| 1290-1298 | Chunk properties setup | ALWAYS | Handle mask phase specially |
| 1299-1301 | `ScopedPaintChunkProperties` | ALWAYS | Set chunk properties |
| 1303-1306 | PaintInfo construction | ALWAYS | Create paint info with cull rect |
| 1308-1319 | Dispatch to painter | CONDITIONAL | Route to appropriate painter |

### Dispatch Logic (lines 1308-1319)

```cpp
if (physical_fragment) {
  // NG path: use BoxFragmentPainter
  BoxFragmentPainter(*physical_fragment).Paint(paint_info);
} else if (const auto* layout_inline = DynamicTo<LayoutInline>(&layout_object)) {
  // Inline path: use InlineBoxFragmentPainter
  InlineBoxFragmentPainter::PaintAllFragments(*layout_inline, fragment_data, fragment_data_idx, paint_info);
} else {
  // Legacy path: use LayoutObject::Paint
  paint_info.SetFragmentDataOverride(&fragment_data);
  paint_layer_.GetLayoutObject().Paint(paint_info);
}
```

---

## 5. PaintLayerPainter::PaintForegroundPhases()

**File:** `paint_layer_painter.cc`
**Lines:** 1361-1382

### Line-by-Line Trace

| Lines | Phase | Conditional | Description |
|-------|-------|-------------|-------------|
| 1363-1364 | `kDescendantBlockBackgroundsOnly` | ALWAYS | Paint descendant backgrounds |
| 1366-1369 | `kForcedColorsModeBackplate` | YES | Backplate for forced colors mode |
| 1371-1374 | `kFloat` | YES | Paint floats (if needed or invalidation checking) |
| 1376 | `kForeground` | ALWAYS | Main foreground content |
| 1378-1381 | `kDescendantOutlinesOnly` | YES | Descendant outlines (if needed or invalidation checking) |

---

## 6. ObjectPainter::PaintOutline()

**File:** `object_painter.cc`
**Lines:** 22-49

### Function Signature
```cpp
void ObjectPainter::PaintOutline(const PaintInfo& paint_info, const PhysicalOffset& paint_offset)
```

### Line-by-Line Trace

| Lines | Function/Check | Conditional | Description |
|-------|----------------|-------------|-------------|
| 23 | Assert phase | ALWAYS | DCHECK should paint self outline |
| 26-30 | Style checks | YES | Skip if no outline or not visible |
| 34-38 | Theme check | YES | Skip if theme draws focus ring |
| 40-45 | Outline rects | YES | Get outline rects, early return if empty |
| 47-48 | `OutlinePainter::PaintOutlineRects()` | ALWAYS | Delegate to OutlinePainter |

---

## 7. ObjectPainter::PaintAllPhasesAtomically()

**File:** `object_painter.cc`
**Lines:** 111-135

### Function Signature
```cpp
void ObjectPainter::PaintAllPhasesAtomically(const PaintInfo& paint_info)
```

### Line-by-Line Trace

| Lines | Function/Check | Conditional | Description |
|-------|----------------|-------------|-------------|
| 115-119 | Selection/text clip check | YES | Direct paint for these phases |
| 121-122 | Foreground check | YES | Only proceed for foreground phase |
| 124-134 | Phase sequence | ALWAYS | Paint all phases in order |

### Phase Sequence
1. `kBlockBackground`
2. `kForcedColorsModeBackplate`
3. `kFloat`
4. `kForeground`
5. `kOutline`

---

## 8. ObjectPainter::RecordHitTestData()

**File:** `object_painter.cc`
**Lines:** 137-158

### Function Signature
```cpp
void ObjectPainter::RecordHitTestData(
    const PaintInfo& paint_info,
    const gfx::Rect& paint_rect,
    const DisplayItemClient& background_client)
```

### Line-by-Line Trace

| Lines | Function/Check | Conditional | Description |
|-------|----------------|-------------|-------------|
| 143-145 | `ShouldOmitCompositingInfo()` | YES | Skip for printing/drag images |
| 150-152 | Visibility check | YES | Skip if not visible |
| 154-158 | RecordHitTestData call | ALWAYS | Record hit test data |

### Hit Test Opaqueness (GetHitTestOpaqueness, lines 160-181)

| Return Value | Condition |
|--------------|-----------|
| `kTransparent` | Not visible to hit testing or frame not visible |
| `kMixed` | Has border radius or is SVG child |
| `kOpaque` | Default case |

---

## 9. ViewPainter::PaintBoxDecorationBackground()

**File:** `view_painter.cc`
**Lines:** 77-247

### Key Decision Points

| Lines | Check | Description |
|-------|-------|-------------|
| 78-80 | Visibility | Skip if not visible |
| 82-87 | Paginated root | Skip for paginated root |
| 89-121 | Paint condition checks | Determine what needs painting |
| 137-147 | Contents space | Adjust for scrollable content |
| 183-205 | Root element state | Compare with document element state |
| 214-219 | PaintRootElementGroup | Paint root background |
| 220-224 | RecordHitTestData | Record hit test data |
| 236-246 | RecordScrollHitTestData | Record scroll hit test |

---

## 10. FramePainter::Paint()

**File:** `frame_painter.cc`
**Lines:** 25-84

### Function Signature
```cpp
void FramePainter::Paint(GraphicsContext& context, PaintFlags paint_flags)
```

### Line-by-Line Trace

| Lines | Function/Check | Conditional | Description |
|-------|----------------|-------------|-------------|
| 26-29 | Privacy check | YES | Skip cross-origin for privacy |
| 31-34 | Throttle/active check | YES | Skip if throttled or inactive |
| 36-42 | Layout view check | YES | Skip if no layout view |
| 46-47 | Layout check | YES | Skip if needs layout |
| 54 | FramePaintTiming | ALWAYS | Timing instrumentation |
| 56-57 | Top-level tracking | ALWAYS | Track if top-level painter |
| 59 | ScopedDisplayItemFragment | ALWAYS | Fragment scope |
| 61 | Get root layer | ALWAYS | Get LayoutView's layer |
| 69-71 | PaintLayerPainter | ALWAYS | Delegate to layer painter |
| 75-77 | Draggable regions | YES | Update if dirty |

---

## Paint Phase Enum Reference

**File:** `paint_phase.h`

| Phase | Value | Description |
|-------|-------|-------------|
| `kBlockBackground` | 0 | Paint backgrounds of current object and non-self-painting descendants |
| `kSelfBlockBackgroundOnly` | 1 | Paint background of current object only |
| `kDescendantBlockBackgroundsOnly` | 2 | Paint backgrounds of non-self-painting descendants only |
| `kForcedColorsModeBackplate` | 3 | Readability backplate for forced colors mode |
| `kFloat` | 4 | Floating objects |
| `kForeground` | 5 | All inlines and atomic elements |
| `kOutline` | 6 | Paint outlines of current object and descendants |
| `kSelfOutlineOnly` | 7 | Paint outline of current object only |
| `kDescendantOutlinesOnly` | 8 | Paint outlines of descendants only |
| `kOverlayOverflowControls` | 9 | Overlay scrollbars |
| `kSelectionDragImage` | 10 | Selection drag image |
| `kTextClip` | 11 | Text clipping |
| `kMask` | 12 | CSS masks |

---

## PaintLayerIteration Flags

**File:** `paint_layer.h`

| Flag | Value | Description |
|------|-------|-------------|
| `kNegativeZOrderChildren` | 1 | Children with negative z-index |
| `kNormalFlowChildren` | 2 | Non-positioned children |
| `kPositiveZOrderChildren` | 4 | Children with z-index >= 0 |
| `kStackedChildren` | 5 | Neg + Pos z-order |
| `kNormalFlowAndPositiveZOrderChildren` | 6 | Normal + Pos z-order |
| `kAllChildren` | 7 | All children |

---

## Graphics Context Calls (Draw Operations)

The following are the terminal draw operations that produce actual display items:

| Location | Call | Description |
|----------|------|-------------|
| ViewPainter | `context.FillRect()` | Background color |
| FrameSetPainter | `context.FillRect()` | Border fill and edges |
| OutlinePainter | Various drawing calls | Outline rendering |
| BoxFragmentPainter | Various drawing calls | Box decoration |

---

## Key Helper Functions

### ShouldCreateSubsequence() (lines 738-808)

Determines if a paint layer should use subsequence caching:
- Returns false for printing/preview
- Returns false if skipping cache
- Returns false if not supporting subsequence
- Returns true if creates new paint chunk
- Returns true if has 2+ descendants
- Returns true if hit test opaqueness mismatch
- Returns true if merged bounds too empty

### PaintedOutputInvisible() (lines 681-707)

Checks if paint output is invisible:
- Returns false if has backdrop filter
- Returns false if has will-change opacity
- Returns false if has opacity animation
- Returns true if opacity < 0.0004f
