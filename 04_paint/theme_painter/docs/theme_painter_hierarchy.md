# ThemePainter Class Hierarchy

## Overview

The ThemePainter system is responsible for rendering native-looking form controls in Chromium's Blink rendering engine. It provides platform-specific theming for HTML form elements like buttons, checkboxes, radio buttons, sliders, progress bars, and text inputs.

## Class Hierarchy

```
ThemePainter (Base Class)
    |
    +-- ThemePainterDefault (Default/Cross-platform Implementation)
```

## Base Class: ThemePainter

**Location:** `third_party/blink/renderer/core/paint/theme_painter.h`

The `ThemePainter` base class defines the interface for painting themed form controls. It provides:

### Public Methods

| Method | Description |
|--------|-------------|
| `Paint()` | Main entry point - dispatches to specific paint methods based on appearance |
| `PaintBorderOnly()` | Paints only the border for certain controls |
| `PaintDecorations()` | Paints decorations like dropdown arrows |
| `PaintSliderTicks()` | Paints tick marks for slider inputs with datalists |
| `PaintCapsLockIndicator()` | Virtual method for caps lock indicator (password fields) |

### Protected Virtual Methods (Control-Specific Painting)

| Method | Default Return | Controls Handled |
|--------|---------------|------------------|
| `PaintCheckbox()` | `true` | `<input type="checkbox">` |
| `PaintRadio()` | `true` | `<input type="radio">` |
| `PaintButton()` | `true` | `<button>`, `<input type="button/submit/reset">` |
| `PaintInnerSpinButton()` | `true` | Number input spin buttons |
| `PaintTextField()` | `true` | `<input type="text">` and similar |
| `PaintTextArea()` | `true` | `<textarea>` |
| `PaintMenuList()` | `true` | `<select>` dropdown |
| `PaintMenuListButton()` | `true` | `<select>` dropdown button |
| `PaintProgressBar()` | `true` | `<progress>` |
| `PaintSliderTrack()` | `true` | `<input type="range">` track |
| `PaintSliderThumb()` | `true` | `<input type="range">` thumb |
| `PaintSearchField()` | `true` | `<input type="search">` |
| `PaintSearchFieldCancelButton()` | `true` | Search field clear button |

## Derived Class: ThemePainterDefault

**Location:** `third_party/blink/renderer/core/paint/theme_painter_default.h`

The `ThemePainterDefault` class provides the actual painting implementation for the default (Aura) theme, which is used on most platforms.

### Key Characteristics

1. **Delegates to WebThemeEngine**: Most painting is delegated to `WebThemeEngineHelper::GetNativeThemeEngine()->Paint()`
2. **Color Scheme Awareness**: Handles light/dark mode and accent colors
3. **Zoom Support**: Applies zoom transformations for high-DPI displays
4. **Forced Colors Mode**: Supports high contrast accessibility modes

### Private Methods

All protected virtual methods from `ThemePainter` are overridden:

```cpp
bool PaintCheckbox(...) override;
bool PaintRadio(...) override;
bool PaintButton(...) override;
bool PaintTextField(...) override;
bool PaintMenuList(...) override;
bool PaintMenuListButton(...) override;
bool PaintSliderTrack(...) override;
bool PaintSliderThumb(...) override;
bool PaintInnerSpinButton(...) override;
bool PaintProgressBar(...) override;
bool PaintTextArea(...) override;
bool PaintSearchField(...) override;
bool PaintSearchFieldCancelButton(...) override;
```

### Helper Methods

| Method | Purpose |
|--------|---------|
| `SetupMenuListArrow()` | Configures dropdown arrow parameters |
| `ApplyZoomToRect()` | Applies zoom transformation to painting rect |

## AppearanceValue Enum (Form Control Types)

The `AppearanceValue` enum determines which painting method is called:

| Value | Control Type | Paint Method |
|-------|--------------|--------------|
| `kCheckbox` | Checkbox | `PaintCheckbox()` |
| `kRadio` | Radio button | `PaintRadio()` |
| `kPushButton` | Push button | `PaintButton()` |
| `kSquareButton` | Square button | `PaintButton()` |
| `kButton` | Generic button | `PaintButton()` |
| `kInnerSpinButton` | Number spinner | `PaintInnerSpinButton()` |
| `kMenulist` | Select dropdown | `PaintMenuList()` |
| `kMenulistButton` | Select dropdown button | `PaintMenuListButton()` |
| `kProgressBar` | Progress indicator | `PaintProgressBar()` |
| `kSliderHorizontal` | Horizontal slider | `PaintSliderTrack()` |
| `kSliderVertical` | Vertical slider | `PaintSliderTrack()` |
| `kSliderThumbHorizontal` | Horizontal slider thumb | `PaintSliderThumb()` |
| `kSliderThumbVertical` | Vertical slider thumb | `PaintSliderThumb()` |
| `kTextField` | Text input | `PaintTextField()` |
| `kTextArea` | Textarea | `PaintTextArea()` |
| `kSearchField` | Search input | `PaintSearchField()` |
| `kSearchFieldCancelButton` | Search clear button | `PaintSearchFieldCancelButton()` |
| `kMeter` | Meter element | Returns `true` (CSS painting) |
| `kListbox` | Listbox | Returns `true` (CSS painting) |
| `kMediaSlider` | Media slider | Returns `true` (CSS painting) |
| `kMediaSliderThumb` | Media slider thumb | Returns `true` (CSS painting) |
| `kMediaVolumeSlider` | Volume slider | Returns `true` (CSS painting) |
| `kMediaVolumeSliderThumb` | Volume slider thumb | Returns `true` (CSS painting) |

## WebThemeEngine Parts

The `WebThemeEngine::Part` enum specifies what to paint:

| Part | Description |
|------|-------------|
| `kPartCheckbox` | Checkbox control |
| `kPartRadio` | Radio button control |
| `kPartButton` | Push button |
| `kPartTextField` | Text input field |
| `kPartMenuList` | Select dropdown |
| `kPartSliderTrack` | Slider track |
| `kPartSliderThumb` | Slider thumb |
| `kPartInnerSpinButton` | Number spinner arrows |
| `kPartProgressBar` | Progress bar |

## WebThemeEngine States

Controls can be in various states:

| State | Description |
|-------|-------------|
| `kStateNormal` | Default state |
| `kStateHover` | Mouse hovering over control |
| `kStatePressed` | Control is being pressed/clicked |
| `kStateDisabled` | Control is disabled |

## Relationship with LayoutTheme

`ThemePainterDefault` holds a reference to `LayoutThemeDefault`:

```cpp
class ThemePainterDefault final : public ThemePainter {
  // ...
  LayoutThemeDefault& theme_;
};
```

This allows access to theme-specific measurements and colors defined in `LayoutTheme`.

## Platform Considerations

- The default implementation (`ThemePainterDefault`) uses the Aura theme engine
- On macOS, there may be platform-specific theming through `ThemePainterMac`
- The `WebThemeEngine` abstraction allows platform-specific native rendering
- Forced colors mode and preferred contrast settings are respected for accessibility
