# Challenges: Compiling Blink Paint to WebAssembly

## Executive Summary

Compiling Blink's paint code to WASM is **feasible but non-trivial**. The core painting logic is relatively clean, but it sits atop a deep dependency graph. The main challenges are:

1. **ComputedStyle** - 3,672 line header with ~500 CSS property accessors
2. **GraphicsContext** - Skia abstraction layer (1,579 lines)
3. **Transitive dependencies** - Each include pulls in more
4. **Generated code** - Some types are code-generated at build time
5. **Oilpan GC markers** - Need to strip or stub

---

## File Size Analysis

### Core Paint Files (what you want)

| File | Lines | Purpose |
|------|-------|---------|
| `box_border_painter.cc` | 2,488 | Border painting logic |
| `box_fragment_painter.cc` | 3,030 | Main box painting |
| `box_painter_base.cc` | 1,548 | Shared painting utilities |
| `pre_paint_tree_walk.cc` | 1,496 | Property tree building |
| `paint_property_tree_builder.cc` | 4,442 | Property tree construction |
| `paint_layer.cc` | 2,752 | Layer management |
| `highlight_painter.cc` | 1,265 | Selection/highlight painting |
| `outline_painter.cc` | 988 | Outline painting |
| **Total core paint** | **~18,000** | |

### Key Dependencies (what you have to deal with)

| File | Lines | Problem |
|------|-------|---------|
| `computed_style.h` | 3,672 | Massive interface, ~500 methods |
| `computed_style_base.h` | **Generated** | Code-generated from CSS properties |
| `graphics_context.h` | 577 | Skia abstraction |
| `graphics_context.cc` | 1,002 | Skia calls |

---

## The Five Biggest Problems

### Problem 1: ComputedStyle is Enormous

`ComputedStyle` has ~500 accessor methods for CSS properties:

```cpp
// computed_style.h - 3,672 lines
class ComputedStyle : public ComputedStyleBase {
  // ... hundreds of these:
  float BorderTopWidth() const;
  Color BorderTopColor() const;
  EBorderStyle BorderTopStyle() const;
  Length MarginTop() const;
  const Font& GetFont() const;
  FilterOperations Filter() const;
  // ... ~500 more
};
```

**The problem**: `box_border_painter.cc` only uses ~30 of these methods, but you can't partially include the class.

**Solution approaches**:

| Approach | Effort | Risk |
|----------|--------|------|
| Create `FakeComputedStyle` with only used methods | Medium | May miss methods |
| Extract interface `IPaintStyle` | High | Requires modifying Blink |
| Include full `ComputedStyle`, stub base | Very High | Dependency explosion |

**Recommended**: Create a shim that wraps your flat style data:

```cpp
// shims/computed_style.h
class ComputedStyle {
  const YourStyleData* data_;
public:
  // Only implement methods actually called by painters
  float BorderTopWidth() const { return data_->border_widths[0]; }
  Color BorderTopColor() const { return Color(data_->border_colors[0]); }
  EBorderStyle BorderTopStyle() const {
    return static_cast<EBorderStyle>(data_->border_styles[0]);
  }
  // ~30 methods for borders
  // ~20 methods for backgrounds
  // ~15 methods for text
  // etc.
};
```

**Estimated stub size**: ~200-400 lines for border painting alone.

---

### Problem 2: GraphicsContext Wraps Skia

`GraphicsContext` is the abstraction layer between paint code and Skia:

```cpp
// graphics_context.h
class GraphicsContext {
  void DrawRect(const gfx::Rect&, const cc::PaintFlags&);
  void DrawRRect(const SkRRect&, const cc::PaintFlags&);
  void FillDRRect(const FloatRoundedRect& outer,
                  const FloatRoundedRect& inner, Color);
  void ClipPath(const SkPath&, AntiAliasingMode);
  void StrokeRect(const gfx::RectF&, const AutoDarkMode&);
  // ~50 more draw methods

  // State management
  void Save();
  void Restore();
  void SetStrokeColor(Color);
  void SetFillColor(Color);
  // etc.
};
```

**The problem**: Do you want to:
- A) Capture abstract ops (then execute elsewhere)
- B) Execute directly via CanvasKit (Skia-in-WASM)
- C) Both

**Solution for capture mode**:

```cpp
// shims/graphics_context.h
class GraphicsContext {
  std::vector<DisplayItem>* output_;
  GraphicsState state_;

public:
  void DrawRect(const gfx::Rect& r, const cc::PaintFlags& f) {
    output_->push_back(DisplayItem{
      .type = DisplayItemType::DrawRect,
      .rect = r,
      .color = f.getColor(),
    });
  }

  void FillDRRect(const FloatRoundedRect& outer,
                  const FloatRoundedRect& inner,
                  Color color) {
    output_->push_back(DisplayItem{
      .type = DisplayItemType::FillDRRect,
      .outer_rrect = outer,
      .inner_rrect = inner,
      .color = color,
    });
  }

  // Must implement ~50 methods
};
```

**Estimated stub size**: ~300-500 lines.

---

### Problem 3: Transitive Include Hell

Each include pulls in more. Here's `box_border_painter.cc`'s direct includes:

```
box_border_painter.cc
├── box_border_painter.h
│   ├── background_bleed_avoidance.h
│   ├── box_sides.h
│   ├── box_strut.h
│   ├── physical_rect.h
│   ├── border_edge.h
│   ├── contoured_rect.h
│   ├── float_rounded_rect.h
│   └── graphics_context.h ──────────────┐
│       ├── cc/paint/paint_flags.h       │
│       ├── Font.h ──────────────────────┼── Font pulls in HarfBuzz, ICU
│       ├── dark_mode_filter.h           │
│       ├── paint_record.h               │
│       └── ... 20+ more                 │
├── computed_style.h ────────────────────┤
│   ├── computed_style_base.h (GENERATED)│
│   ├── 50+ style-related includes       │
│   └── ... pulls in CSS parser, etc.    │
├── box_painter.h                        │
├── contoured_border_geometry.h          │
├── object_painter.h                     │
└── skia/SkPath.h, SkPathBuilder.h       │
```

**The problem**: You can't just compile `box_border_painter.cc` - you need to provide all transitive dependencies.

**Solution**: Create a parallel include tree with stubs:

```
shims/
├── third_party/
│   └── blink/
│       └── renderer/
│           ├── core/
│           │   ├── style/
│           │   │   └── computed_style.h  (your stub)
│           │   └── paint/
│           │       └── paint_auto_dark_mode.h (stub)
│           └── platform/
│               └── graphics/
│                   └── graphics_context.h (your stub)
└── ui/
    └── gfx/
        └── geometry/
            └── rect.h (can often use real gfx)
```

Then compile with: `emcc -I shims/ -I chromium/src/ ...`

Your shims shadow the real includes.

---

### Problem 4: Generated Code (ComputedStyleBase)

`ComputedStyle` inherits from `ComputedStyleBase`, which is **generated at build time** from CSS property definitions:

```
# In chromium build:
css_properties.json5 → make_computed_style_base.py → computed_style_base.h
```

This generated file contains:
- Storage for all CSS property values
- Getters/setters for each property
- Comparison operators
- Inheritance logic

**The problem**: You don't have `computed_style_base.h` without running Chromium's build.

**Solutions**:

| Approach | Effort | Notes |
|----------|--------|-------|
| Run Chromium build once, extract generated file | Low | One-time, may go stale |
| Stub `ComputedStyleBase` entirely | Medium | Only provide what you need |
| Port the generator script | High | Overkill |

**Recommended**: Stub it:

```cpp
// shims/computed_style_base.h
class ComputedStyleBase {
protected:
  // Empty - all real accessors in your ComputedStyle shim
};
```

---

### Problem 5: Oilpan GC Markers

Blink uses the Oilpan garbage collector. Code is littered with:

```cpp
class Foo : public GarbageCollected<Foo> {
  Member<Bar> bar_;           // GC-traced pointer
  Persistent<Baz> baz_;       // Root pointer
};

// In headers:
STACK_ALLOCATED();            // Prevents heap allocation
DISALLOW_NEW();               // Prevents new
```

**In box_border_painter**:

```cpp
class BoxBorderPainter {
  STACK_ALLOCATED();   // ← Must handle this
  // ...
};

struct OpacityGroup {
  DISALLOW_NEW();      // ← And this
  // ...
};
```

**Solution**: Define them as no-ops:

```cpp
// shims/wtf/allocator/allocator.h
#define STACK_ALLOCATED()
#define DISALLOW_NEW()
#define GC_PLUGIN_IGNORE(reason)

template<typename T>
using Member = T*;      // Just a raw pointer

template<typename T>
using Persistent = T*;
```

**Estimated stub size**: ~20 lines.

---

## Dependency Graph Visualization

```
                    ┌─────────────────────────────────────────────────────┐
                    │              YOUR WASM MODULE                        │
                    └─────────────────────────────────────────────────────┘
                                          │
                         ┌────────────────┼────────────────┐
                         │                │                │
                         ▼                ▼                ▼
              ┌──────────────────┐ ┌─────────────┐ ┌──────────────────┐
              │ box_border_      │ │ box_        │ │ paint_property_  │
              │ painter.cc       │ │ fragment_   │ │ tree_builder.cc  │
              │ (2,488 lines)    │ │ painter.cc  │ │ (4,442 lines)    │
              │ KEEP             │ │ (3,030)     │ │ KEEP             │
              └────────┬─────────┘ │ KEEP        │ └────────┬─────────┘
                       │           └──────┬──────┘          │
    ┌──────────────────┴──────────────────┴─────────────────┴─────────────┐
    │                         SHARED DEPENDENCIES                          │
    ├─────────────────────────────────────────────────────────────────────┤
    │                                                                      │
    │  ┌─────────────────────┐     ┌─────────────────────┐                │
    │  │ ComputedStyle       │     │ GraphicsContext     │                │
    │  │ (3,672 lines)       │     │ (1,579 lines)       │                │
    │  │                     │     │                     │                │
    │  │ STUB: ~300 lines    │     │ STUB: ~400 lines    │                │
    │  └──────────┬──────────┘     └──────────┬──────────┘                │
    │             │                           │                           │
    │             ▼                           ▼                           │
    │  ┌─────────────────────┐     ┌─────────────────────┐                │
    │  │ ComputedStyleBase   │     │ Skia types          │                │
    │  │ (GENERATED)         │     │ SkPath, SkRRect     │                │
    │  │                     │     │                     │                │
    │  │ STUB: ~50 lines     │     │ USE REAL SKIA       │                │
    │  └─────────────────────┘     │ (compiles to WASM)  │                │
    │                              └─────────────────────┘                │
    │                                                                      │
    │  ┌─────────────────────┐     ┌─────────────────────┐                │
    │  │ gfx:: geometry      │     │ WTF:: containers    │                │
    │  │ Rect, Point, etc.   │     │ Vector, String      │                │
    │  │                     │     │                     │                │
    │  │ USE REAL or stub    │     │ USE std:: or stub   │                │
    │  └─────────────────────┘     └─────────────────────┘                │
    │                                                                      │
    └─────────────────────────────────────────────────────────────────────┘
```

---

## Effort Estimate

### Minimum Viable Border Painter

| Component | Lines to Write | Notes |
|-----------|----------------|-------|
| `ComputedStyle` shim | ~300 | Border methods only |
| `GraphicsContext` shim | ~400 | Capture mode |
| `ComputedStyleBase` shim | ~50 | Empty base |
| Oilpan/WTF stubs | ~50 | Macros + typedefs |
| Geometry type stubs | ~100 | Or use real gfx:: |
| Build configuration | ~50 | CMake/emscripten |
| **Total shim code** | **~950** | |

Then you get the **2,488 lines of border painting logic for free**.

### Full Paint System

| Component | Lines to Write | Notes |
|-----------|----------------|-------|
| All shims above | ~950 | |
| Expand `ComputedStyle` | +500 | Backgrounds, text, etc. |
| Expand `GraphicsContext` | +200 | More draw methods |
| `PhysicalFragment` shim | ~150 | Layout geometry |
| `PaintController` shim | ~200 | Display list management |
| `PropertyTree` types | ~300 | Transform/Clip/Effect nodes |
| **Total shim code** | **~2,300** | |

This unlocks **~18,000 lines** of battle-tested paint code.

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Missing method in shim | High | Low | Compiler error, easy to add |
| Semantic mismatch | Medium | Medium | Test against real Chromium |
| Skia version mismatch | Medium | High | Pin to same Skia version |
| Generated code changes | Low | Medium | Re-extract periodically |
| Performance issues | Low | Low | WASM is fast enough |

---

## Recommended Approach

1. **Start with `box_border_painter.cc` only** - smallest surface area
2. **Create minimal shims** - only stub what's actually called
3. **Use real gfx:: and Skia** - they compile cleanly
4. **Capture to abstract ops** - don't link Skia to WASM initially
5. **Test one border style** - solid borders first, then expand
6. **Iterate** - add methods as compiler errors reveal them

### First Compilation Target

```bash
# Goal: Compile box_border_painter.cc to WASM
emcc \
  -I shims/ \
  -I chromium/src/ \
  -I chromium/src/third_party/skia/include/ \
  -std=c++20 \
  -DNDEBUG \
  shims/*.cc \
  chromium/src/third_party/blink/renderer/core/paint/box_border_painter.cc \
  -o border_painter.wasm
```

You'll get ~50 errors on first try. Each error tells you what to stub next.

---

## Summary

| Aspect | Assessment |
|--------|------------|
| **Feasibility** | Yes, with ~2,000 lines of shims |
| **Main blockers** | ComputedStyle (huge), GraphicsContext (Skia) |
| **Easiest win** | Border painting - self-contained logic |
| **Hardest part** | Getting first successful compile |
| **Time estimate** | 2-4 weeks for border painter, 2-3 months for full paint |
| **Lines of free code** | ~18,000 (paint logic you don't write) |
