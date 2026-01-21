# Chromium Text Shaping Reference Code

This directory contains copies of the key Chromium source files that handle text shaping and post-shaping adjustments. These files are referenced to ensure our CanvasKit-based text shaping replication matches Chromium's behavior.

## Files

### shape_result_spacing.cc / shape_result_spacing.h

The core logic for applying CSS letter-spacing and word-spacing to shaped text.

**Key function: `ComputeSpacing(unsigned index, bool is_cursive_script)`**

This returns the spacing to add AFTER a glyph at the given index:

1. **letter_spacing** - Added after every character EXCEPT:
   - Zero-width characters (via `Character::TreatAsZeroWidthSpace()`)
   - In cursive scripts (if `IgnoreLetterSpacingInCursiveScriptsEnabled`)

2. **word_spacing** - Added after space characters including:
   - Regular space (U+0020)
   - Non-breaking space (U+00A0) - always eligible
   - Other characters treated as space via `Character::TreatAsSpace()`

### harfbuzz_shaper.cc / harfbuzz_shaper.h

The HarfBuzz integration that performs the initial text shaping. HarfBuzz handles:
- Glyph selection (substitution via GSUB table)
- Glyph positioning (kerning, positioning via GPOS table)
- Complex script shaping (Arabic joining, Indic conjuncts, etc.)

### shape_result.cc / shape_result.h

Container for shaped text results. Stores:
- Glyph IDs
- Glyph advances and offsets
- Character-to-glyph mapping
- Total width

## Post-Shaping Adjustments in Chromium

The order of operations in Chromium:

1. **HarfBuzz shaping** → base glyph positions with kerning
2. **Letter-spacing** → added after each non-zero-width glyph
3. **Word-spacing** → added after space characters
4. **Justification** → distributed across expansion opportunities

## Matching CanvasKit

CanvasKit's Paragraph API uses the same HarfBuzz library and supports:
- `letterSpacing` in TextStyle
- `wordSpacing` in TextStyle

These are applied similarly to Chromium's approach.
