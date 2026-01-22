// Test to verify Skia's saveLayer with DstIn blend mode for gradient text

#include <cstdio>
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkData.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_mac_ct.h"
#include "include/effects/SkGradient.h"
#include "include/encode/SkPngEncoder.h"
#include "include/core/SkStream.h"

int main() {
    // Create a 400x100 surface
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 100));
    auto canvas = surface->getCanvas();
    canvas->clear(SK_ColorWHITE);

    printf("Testing Skia saveLayer with DstIn blend mode for TEXT...\n");
    printf("SkBlendMode::kDstIn = %d\n", (int)SkBlendMode::kDstIn);

    // Load Arial.ttf from parent directory using CoreText font manager
    sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_CoreText(nullptr);
    sk_sp<SkData> fontData = SkData::MakeFromFileName("../Arial.ttf");
    if (!fontData) {
        printf("ERROR: Could not load ../Arial.ttf\n");
        return 1;
    }
    sk_sp<SkTypeface> typeface = fontMgr->makeFromData(fontData);
    if (!typeface) {
        printf("ERROR: Could not create typeface from Arial.ttf\n");
        return 1;
    }

    SkFont font(typeface, 36);
    font.setEdging(SkFont::Edging::kAntiAlias);

    // Measure text to get bounds
    const char* text = "Gradient Text";
    auto textBlob = SkTextBlob::MakeFromString(text, font);
    SkRect textBounds = textBlob->bounds();

    printf("Text bounds: %.1f, %.1f, %.1f, %.1f\n",
           textBounds.left(), textBounds.top(), textBounds.right(), textBounds.bottom());

    // Position text
    float textX = 50;
    float textY = 60;

    // Calculate gradient rect to cover text area
    float gradLeft = textX + textBounds.left() - 10;
    float gradTop = textY + textBounds.top() - 10;
    float gradRight = textX + textBounds.right() + 10;
    float gradBottom = textY + textBounds.bottom() + 10;

    // =============================================
    // Gradient text pattern (like Chromium does)
    // =============================================

    // 1. Outer saveLayer (SrcOver - isolate the compositing)
    SkPaint outerPaint;
    canvas->saveLayer(nullptr, &outerPaint);
    printf("1. saveLayer (SrcOver)\n");

    // 2. Draw gradient rect (this is the "destination" for DstIn)
    SkPoint gradientPts[] = {{gradLeft, textY}, {gradRight, textY}};
    SkColor4f gradientColors[] = {
        SkColor4f{0.4f, 0.494f, 0.918f, 1.0f},   // #667EEA - blue
        SkColor4f{0.463f, 0.294f, 0.635f, 1.0f}, // #764BA2 - purple
        SkColor4f{0.941f, 0.576f, 0.984f, 1.0f}  // #F093FB - pink
    };
    float positions[] = {0.0f, 0.5f, 1.0f};

    SkGradient::Colors gradColors(
        SkSpan<const SkColor4f>(gradientColors, 3),
        SkSpan<const float>(positions, 3),
        SkTileMode::kClamp
    );
    SkGradient gradient(gradColors, {});
    auto shader = SkShaders::LinearGradient(gradientPts, gradient);

    SkPaint gradPaint;
    gradPaint.setShader(shader);
    gradPaint.setAntiAlias(true);
    canvas->drawRect(SkRect::MakeLTRB(gradLeft, gradTop, gradRight, gradBottom), gradPaint);
    printf("2. drawRect with gradient (%.1f, %.1f, %.1f, %.1f)\n", gradLeft, gradTop, gradRight, gradBottom);

    // 3. SaveLayer with DstIn blend mode
    SkPaint dstInPaint;
    dstInPaint.setBlendMode(SkBlendMode::kDstIn);
    canvas->saveLayer(nullptr, &dstInPaint);
    printf("3. saveLayer (DstIn, blendMode=%d)\n", (int)SkBlendMode::kDstIn);

    // 4. Draw text as "mask" (black text - alpha matters for DstIn)
    // DstIn: keeps destination (gradient) where source (text) has alpha
    SkPaint textPaint;
    textPaint.setColor(SK_ColorBLACK);
    textPaint.setAntiAlias(true);
    canvas->drawTextBlob(textBlob, textX, textY, textPaint);
    printf("4. drawTextBlob at (%.1f, %.1f)\n", textX, textY);

    // 5. Restore DstIn layer - this composites with DstIn
    canvas->restore();
    printf("5. restore (DstIn compositing)\n");

    // 6. Restore outer layer
    canvas->restore();
    printf("6. restore (SrcOver)\n");

    // Save to PNG
    auto image = surface->makeImageSnapshot();
    SkFILEWStream file("gradient_mask_test.png");

    SkPixmap pixmap;
    if (image->peekPixels(&pixmap)) {
        if (SkPngEncoder::Encode(&file, pixmap, {})) {
            printf("\nWrote gradient_mask_test.png\n");
            printf("\nEXPECTED: Text with gradient fill (blue->purple->pink)\n");
            printf("          White background around the text\n");
        } else {
            printf("ERROR: Failed to encode PNG\n");
        }
    }

    return 0;
}
