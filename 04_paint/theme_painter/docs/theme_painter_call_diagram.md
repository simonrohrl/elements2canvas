# ThemePainter Call Diagram

## High-Level Paint Flow

```
PaintLayerPainter::Paint()
    |
    v
PaintLayerPainter::PaintWithPhase(PaintPhase::kForeground)
    |
    v
PaintLayerPainter::PaintFragmentWithPhase()
    |
    v
BoxFragmentPainter::Paint() / LayoutObject::Paint()
    |
    v
BoxPainter::Paint() / BoxModelObjectPainter
    |
    v
ThemePainter::Paint()  <-- Entry point for themed controls
```

## ThemePainter::Paint() Dispatch Flow

```
ThemePainter::Paint(LayoutObject&, PaintInfo&, gfx::Rect&)
    |
    |-- Get AppearanceValue from style.EffectiveAppearance()
    |
    +-- switch(appearance)
        |
        |-- kCheckbox ---------> PaintCheckbox()
        |-- kRadio ------------> PaintRadio()
        |-- kPushButton -------> PaintButton()
        |-- kSquareButton -----> PaintButton()
        |-- kButton -----------> PaintButton()
        |-- kInnerSpinButton --> PaintInnerSpinButton()
        |-- kMenulist ---------> PaintMenuList()
        |-- kProgressBar ------> PaintProgressBar()
        |-- kSliderHorizontal -> PaintSliderTrack()
        |-- kSliderVertical ---> PaintSliderTrack()
        |-- kSliderThumbHorizontal -> PaintSliderThumb()
        |-- kSliderThumbVertical ---> PaintSliderThumb()
        |-- kTextField --------> PaintTextField()
        |-- kTextArea ---------> PaintTextArea()
        |-- kSearchField ------> PaintSearchField()
        |-- kSearchFieldCancelButton -> PaintSearchFieldCancelButton()
        |-- kMeter ------------> return true (CSS painting)
        |-- kMediaSlider ------> return true (CSS painting)
        |-- kMenulistButton ---> return true (decorations only)
        |-- kListbox ----------> return true (CSS painting)
        |-- default -----------> return true (CSS painting)
```

## ThemePainterDefault Control Painting Flow

### Checkbox Painting

```
ThemePainterDefault::PaintCheckbox()
    |
    |-- Create WebThemeEngine::ButtonExtraParams
    |   |-- button.checked = IsChecked(element)
    |   |-- button.indeterminate = IsIndeterminate(element)
    |   |-- button.zoom = style.EffectiveZoom()
    |
    |-- ApplyZoomToRect() if zoom != 1
    |   |-- GraphicsContextStateSaver
    |   |-- context.Translate()
    |   |-- context.Scale()
    |
    |-- Calculate color_scheme for accent color contrast
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Canvas: paint_info.context.Canvas()
        |-- Part: WebThemeEngine::kPartCheckbox
        |-- State: GetWebThemeState(element)
        |-- Rect: unzoomed_rect
        |-- ExtraParams: button
        |-- ForcedColorsMode
        |-- ColorScheme
        |-- PreferredContrast
        |-- ColorProvider
        |-- AccentColor
```

### Radio Button Painting

```
ThemePainterDefault::PaintRadio()
    |
    |-- Create WebThemeEngine::ButtonExtraParams
    |   |-- button.checked = IsChecked(element)
    |   |-- button.zoom = style.EffectiveZoom()
    |
    |-- ApplyZoomToRect()
    |-- Calculate color_scheme for accent color contrast
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Part: WebThemeEngine::kPartRadio
```

### Button Painting

```
ThemePainterDefault::PaintButton()
    |
    |-- Create WebThemeEngine::ButtonExtraParams
    |   |-- button.has_border = true
    |   |-- button.zoom = style.EffectiveZoom()
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Part: WebThemeEngine::kPartButton
```

### TextField Painting

```
ThemePainterDefault::PaintTextField()
    |
    |-- Check style.HasBorderRadius() || style.HasBackgroundImage()
    |   +-- return true (let CSS paint)
    |
    |-- Create WebThemeEngine::TextFieldExtraParams
    |   |-- text_field.is_text_area
    |   |-- text_field.is_listbox
    |   |-- text_field.has_border = true
    |   |-- text_field.zoom
    |   |-- text_field.background_color
    |   |-- text_field.auto_complete_active
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Part: WebThemeEngine::kPartTextField
```

### MenuList (Select Dropdown) Painting

```
ThemePainterDefault::PaintMenuList()
    |
    |-- Create WebThemeEngine::MenuListExtraParams
    |   |-- menu_list.has_border
    |   |-- menu_list.has_border_radius
    |   |-- menu_list.zoom
    |   |-- menu_list.background_color
    |   |-- menu_list.fill_content_area
    |
    |-- SetupMenuListArrow()
    |   |-- Calculate arrow_direction (Down/Right/Left)
    |   |-- Calculate arrow_x, arrow_y, arrow_size
    |   |-- Set arrow_color
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Part: WebThemeEngine::kPartMenuList
```

### Slider Track Painting

```
ThemePainterDefault::PaintSliderTrack()
    |
    |-- Create WebThemeEngine::SliderExtraParams
    |   |-- slider.vertical
    |   |-- slider.in_drag = false
    |   |-- slider.zoom
    |   |-- slider.thumb_x, slider.thumb_y
    |   |-- slider.right_to_left
    |
    |-- PaintSliderTicks() for datalist ticks
    |   |-- context.FillRect() for each tick
    |
    |-- Calculate color_scheme for accent color contrast
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Part: WebThemeEngine::kPartSliderTrack
```

### Slider Thumb Painting

```
ThemePainterDefault::PaintSliderThumb()
    |
    |-- Create WebThemeEngine::SliderExtraParams
    |   |-- slider.vertical
    |   |-- slider.in_drag = element.IsActive()
    |   |-- slider.zoom
    |
    |-- Get accent_color from host input
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Part: WebThemeEngine::kPartSliderThumb
```

### Progress Bar Painting

```
ThemePainterDefault::PaintProgressBar()
    |
    |-- Calculate value_rect
    |   |-- Determinate: DeterminateProgressValueRectFor()
    |   |-- Indeterminate: IndeterminateProgressValueRectFor()
    |
    |-- Create WebThemeEngine::ProgressBarExtraParams
    |   |-- progress_bar.determinate
    |   |-- progress_bar.value_rect_x/y/width/height
    |   |-- progress_bar.zoom
    |   |-- progress_bar.is_horizontal
    |
    |-- DirectionFlippingScope (for RTL/vertical)
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Part: WebThemeEngine::kPartProgressBar
```

### Inner Spin Button Painting

```
ThemePainterDefault::PaintInnerSpinButton()
    |
    |-- Create WebThemeEngine::InnerSpinButtonExtraParams
    |   |-- inner_spin.spin_up
    |   |-- inner_spin.read_only
    |   |-- inner_spin.spin_arrows_direction
    |
    +-- WebThemeEngineHelper::GetNativeThemeEngine()->Paint()
        |-- Part: WebThemeEngine::kPartInnerSpinButton
```

### Search Field Cancel Button Painting

```
ThemePainterDefault::PaintSearchFieldCancelButton()
    |
    |-- Calculate content_box and cancel_button_size
    |-- Calculate painting_rect (centered)
    |
    |-- Select appropriate image based on:
    |   |-- PreferredContrast (high contrast mode)
    |   |-- ColorScheme (light/dark)
    |   |-- IsActive() (pressed state)
    |
    +-- paint_info.context.DrawImage()
        |-- target_image
        |-- Image::kSyncDecode
        |-- ImageAutoDarkMode::Disabled()
        |-- ImagePaintTimingInfo()
        |-- painting_rect
```

## Direction Flipping (RTL Support)

```
DirectionFlippingScope::DirectionFlippingScope()
    |
    |-- Check writing direction
    |   |-- InlineEnd == PhysicalDirection::kLeft -> horizontal flip
    |   |-- InlineEnd == PhysicalDirection::kUp -> vertical flip
    |
    |-- If horizontal flip:
    |   |-- context.Save()
    |   |-- context.Translate(2 * rect.x() + rect.width(), 0)
    |   |-- context.Scale(-1, 1)
    |
    |-- If vertical flip:
    |   |-- context.Save()
    |   |-- context.Translate(0, 2 * rect.y() + rect.height())
    |   |-- context.Scale(1, -1)

~DirectionFlippingScope()
    |
    +-- context.Restore() (if flip was applied)
```

## Zoom Application

```
ThemePainterDefault::ApplyZoomToRect()
    |
    |-- If zoom_level != 1:
    |   |-- state_saver.Save()
    |   |-- Adjust unzoomed_rect dimensions
    |   |-- context.Translate(unzoomed_rect.x(), unzoomed_rect.y())
    |   |-- context.Scale(zoom_level, zoom_level)
    |   |-- context.Translate(-unzoomed_rect.x(), -unzoomed_rect.y())
    |
    +-- Return unzoomed_rect
```

## Slider Ticks Painting (DataList Support)

```
ThemePainter::PaintSliderTicks()
    |
    |-- Validate: HTMLInputElement with type=range and datalist
    |
    |-- Get thumb and track layout objects
    |
    |-- Calculate tick dimensions and position
    |   |-- tick_size from LayoutTheme
    |   |-- tick_offset_from_center
    |   |-- tick_region boundaries
    |
    |-- For each datalist option:
    |   |-- Parse value and calculate position
    |   |-- Set tick_rect position
    |   +-- paint_info.context.FillRect(tick_rect, color, auto_dark_mode)
```

## WebThemeEngine State Determination

```
GetWebThemeState(element)
    |
    |-- IsDisabledFormControl() -> kStateDisabled
    |-- IsActive() -> kStatePressed
    |-- IsHovered() && hover_available -> kStateHover
    +-- Otherwise -> kStateNormal
```

## Accent Color Calculation

```
GetAccentColor(style, document, clamping_behavior)
    |
    |-- Check style.AccentColorResolved()
    |-- Check system accent color (if enabled)
    |   |-- Optional clamping for contrast
    |
    |-- Handle transparency:
    |   |-- Composite on Canvas background color
    |
    +-- Return SkColor or nullopt

GetColorSchemeForAccentColor()
    |
    |-- Calculate contrast with light/dark backgrounds
    |-- Return color_scheme that provides best contrast
```
