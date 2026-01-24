// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

#include "base/logging.h"
#include "cc/layers/view_transition_content_layer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_flags.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_layer_stacking_node.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_serializer.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_effectively_invisible.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/subsequence_recorder.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/point3_f.h"

namespace blink {

namespace {

constexpr char kDevToolsTimelineCategory[] = "devtools.timeline";

class PaintTimelineReporter {
  STACK_ALLOCATED();

 public:
  PaintTimelineReporter(const PaintLayer& layer, bool should_paint_content)
      : layer_(layer) {
    if (ShouldReport(should_paint_content)) {
      reset_current_reporting_.emplace(&current_reporting_, this);
      TRACE_EVENT_BEGIN1(
          kDevToolsTimelineCategory, "Paint", "data",
          [&layer](perfetto::TracedValue context) {
            const LayoutObject& object = layer.GetLayoutObject();
            gfx::Rect cull_rect =
                object.FirstFragment().GetContentsCullRect().Rect();
            // Convert the cull rect into the local coordinates of layer.
            cull_rect.Offset(
                -ToRoundedVector2d(object.FirstFragment().PaintOffset()));
            inspector_paint_event::Data(std::move(context), object.GetFrame(),
                                        &object, cull_rect);
          });
    }
  }

  ~PaintTimelineReporter() {
    if (current_reporting_ == this) {
      TRACE_EVENT_END0(kDevToolsTimelineCategory, "Paint");
    }
  }

 private:
  bool ShouldReport(bool should_paint_content) const {
    if (!TRACE_EVENT_CATEGORY_ENABLED(kDevToolsTimelineCategory)) {
      return false;
    }
    // Always report for the top layer to cover the cost of tree walk and
    // cache copying of non-repainted contents.
    if (!current_reporting_) {
      return true;
    }
    if (!should_paint_content) {
      return false;
    }
    if (!layer_.SelfNeedsRepaint()) {
      return false;
    }
    if (!current_reporting_->layer_.SelfNeedsRepaint()) {
      return true;
    }
    // The layer should report if it has an expanded cull rect.
    if (const auto* properties =
            layer_.GetLayoutObject().FirstFragment().PaintProperties()) {
      if (const auto* scroll = properties->Scroll()) {
        if (CullRect::CanExpandForScroll(*scroll)) {
          return true;
        }
      }
      if (properties->ForNodes<TransformPaintPropertyNode>(
              [](const TransformPaintPropertyNode& node) {
                return node.RequiresCullRectExpansion();
              })) {
        return true;
      }
    }
    return false;
  }

  static PaintTimelineReporter* current_reporting_;
  const PaintLayer& layer_;
  std::optional<base::AutoReset<PaintTimelineReporter*>>
      reset_current_reporting_;
};

PaintTimelineReporter* PaintTimelineReporter::current_reporting_ = nullptr;

// Check if the specified node is the scope for a (non-document) scoped view
// transition, and if so return the scope snapshot layer, which is used to
// "pause" the rendering inside the scope during the callback.
scoped_refptr<cc::ViewTransitionContentLayer> GetTransitionScopeSnapshotLayer(
    const Node* scope) {
  const Element* scope_element = DynamicTo<Element>(scope);
  if (!scope_element || scope_element->IsDocumentElement() ||
      scope_element->IsPseudoElement()) {
    // Ignore document transitions here, since they use different mechanisms to
    // pause rendering. (Main frame or local root document transitions pause the
    // compositor with ProxyMain::SetPauseRendering, and same-site subframe
    // document transitions paint the snapshot in EmbeddedContentPainter.)
    return nullptr;
  }
  if (auto* transition = ViewTransitionUtils::GetTransition(*scope_element)) {
    return transition->GetScopeSnapshotLayer();
  }
  return nullptr;
}

// Helper to get a debug name for a LayoutObject
String GetLayoutObjectDebugName(const LayoutObject& object) {
  StringBuilder name;
  name.Append(object.DebugName());
  if (Node* node = object.GetNode()) {
    if (Element* element = DynamicTo<Element>(node)) {
      if (element->HasID()) {
        name.Append("#");
        name.Append(element->GetIdAttribute());
      }
      if (element->HasClass()) {
        const SpaceSplitString& classes = element->ClassNames();
        for (wtf_size_t i = 0; i < classes.size(); ++i) {
          name.Append(".");
          name.Append(classes[i]);
        }
      }
    }
  }
  return name.ToString();
}

// Global counters for generating unique IDs during a single logging pass
static int g_layout_object_id_counter = 0;
static int g_paint_layer_id_counter = 0;

// Serialize ComputedStyle to JSON
std::unique_ptr<JSONObject> SerializeComputedStyle(const ComputedStyle& style) {
  auto json = std::make_unique<JSONObject>();

  // Position and display
  json->SetString("display", style.Display() == EDisplay::kNone ? "none" :
                             style.Display() == EDisplay::kBlock ? "block" :
                             style.Display() == EDisplay::kInline ? "inline" :
                             style.Display() == EDisplay::kInlineBlock ? "inline-block" :
                             style.Display() == EDisplay::kFlex ? "flex" :
                             style.Display() == EDisplay::kInlineFlex ? "inline-flex" :
                             style.Display() == EDisplay::kGrid ? "grid" :
                             style.Display() == EDisplay::kInlineGrid ? "inline-grid" :
                             style.Display() == EDisplay::kContents ? "contents" :
                             "other");
  json->SetString("position", style.GetPosition() == EPosition::kStatic ? "static" :
                              style.GetPosition() == EPosition::kRelative ? "relative" :
                              style.GetPosition() == EPosition::kAbsolute ? "absolute" :
                              style.GetPosition() == EPosition::kFixed ? "fixed" :
                              style.GetPosition() == EPosition::kSticky ? "sticky" :
                              "other");

  // Visual properties
  json->SetDouble("opacity", style.Opacity());
  json->SetInteger("z_index", style.EffectiveZIndex());

  // Transform
  json->SetBoolean("has_transform", style.HasTransform());

  // Overflow
  json->SetString("overflow_x", style.OverflowX() == EOverflow::kVisible ? "visible" :
                                style.OverflowX() == EOverflow::kHidden ? "hidden" :
                                style.OverflowX() == EOverflow::kScroll ? "scroll" :
                                style.OverflowX() == EOverflow::kAuto ? "auto" :
                                style.OverflowX() == EOverflow::kClip ? "clip" :
                                "other");
  json->SetString("overflow_y", style.OverflowY() == EOverflow::kVisible ? "visible" :
                                style.OverflowY() == EOverflow::kHidden ? "hidden" :
                                style.OverflowY() == EOverflow::kScroll ? "scroll" :
                                style.OverflowY() == EOverflow::kAuto ? "auto" :
                                style.OverflowY() == EOverflow::kClip ? "clip" :
                                "other");

  // Visibility
  json->SetString("visibility", style.Visibility() == EVisibility::kVisible ? "visible" :
                                style.Visibility() == EVisibility::kHidden ? "hidden" :
                                style.Visibility() == EVisibility::kCollapse ? "collapse" :
                                "other");

  // Filters and effects
  json->SetBoolean("has_filter", style.HasFilter());
  json->SetBoolean("has_backdrop_filter", style.HasNonInitialBackdropFilter());
  json->SetBoolean("has_clip_path", style.HasClipPath());
  json->SetBoolean("has_mask", style.HasMask());

  // Blend mode
  if (style.HasBlendMode()) {
    json->SetBoolean("has_blend_mode", true);
  }

  // Isolation (check via Isolation() enum)
  json->SetBoolean("is_isolated", style.Isolation() == EIsolation::kIsolate);

  // Will-change
  json->SetBoolean("has_will_change_transform", style.HasWillChangeTransformHint());
  json->SetBoolean("has_will_change_opacity", style.HasWillChangeOpacityHint());

  // Contain property
  json->SetBoolean("contain_paint", style.ContainsPaint());
  json->SetBoolean("contain_layout", style.ContainsLayout());

  // Typography properties for text shaping reproduction
  json->SetDouble("letter_spacing", style.LetterSpacing());
  json->SetDouble("word_spacing", style.WordSpacing());

  // Font properties
  const FontDescription& font = style.GetFontDescription();
  json->SetDouble("font_size", font.ComputedSize());
  json->SetString("font_family", font.Family().FamilyName().GetString());
  json->SetInteger("font_weight", static_cast<int>(font.Weight()));
  json->SetString("font_style",
      font.Style() == kItalicSlopeValue ? "italic" :
      font.Style() > kNormalSlopeValue ? "oblique" : "normal");

  return json;
}

std::unique_ptr<JSONObject> SerializeLayoutObject(const LayoutObject& object,
                                                   int id) {
  auto json = std::make_unique<JSONObject>();
  json->SetInteger("id", id);
  json->SetString("name", GetLayoutObjectDebugName(object));
  json->SetInteger("z_index", object.StyleRef().EffectiveZIndex());
  json->SetBoolean("is_stacking_context", object.IsStackingContext());
  json->SetBoolean("is_stacked", object.IsStacked());
  json->SetBoolean("has_layer", object.HasLayer());

  // Add is_self_painting for objects with layers
  if (object.HasLayer()) {
    json->SetBoolean("is_self_painting",
      To<LayoutBoxModelObject>(object).Layer()->IsSelfPaintingLayer());
  }

  if (Node* node = object.GetNode()) {
    if (node->IsElementNode()) {
      json->SetString("tag", To<Element>(node)->TagQName().ToString());
    }
  }

  // Add computed style information
  json->SetObject("computed_style", SerializeComputedStyle(object.StyleRef()));

  // Serialize text content and shape results for LayoutText objects
  if (object.IsText()) {
    const LayoutText& layout_text = To<LayoutText>(object);

    // Add the text content
    json->SetString("text", layout_text.TransformedText());

    // Serialize text fragments with glyph data using NG layout's InlineCursor
    auto fragments_array = std::make_unique<JSONArray>();

    // Use InlineCursor to iterate over FragmentItems for this LayoutText
    InlineCursor cursor;
    cursor.MoveTo(layout_text);

    for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
      const FragmentItem* item = cursor.CurrentItem();
      if (!item || item->Type() != FragmentItem::kText)
        continue;

      auto frag_json = std::make_unique<JSONObject>();

      // Geometry (in containing block coordinates)
      const PhysicalRect rect = item->RectInContainerFragment();
      frag_json->SetDouble("x", rect.X().ToDouble());
      frag_json->SetDouble("y", rect.Y().ToDouble());
      frag_json->SetDouble("width", rect.Width().ToDouble());
      frag_json->SetDouble("height", rect.Height().ToDouble());

      // Text range within the LayoutText's content
      frag_json->SetInteger("start", item->StartOffset());
      frag_json->SetInteger("end", item->EndOffset());

      // Serialize shape result (glyph data)
      const ShapeResultView* shape = item->TextShapeResult();
      if (shape) {
        auto runs_array = std::make_unique<JSONArray>();
        auto run_json = std::make_unique<JSONObject>();

        // Collect glyph IDs and positions using ForEachGlyph callback
        struct GlyphCollector {
          Vector<uint16_t> glyphs;
          Vector<float> positions;

          static void Callback(void* context,
                               unsigned character_index,
                               Glyph glyph,
                               gfx::Vector2dF glyph_offset,
                               float total_advance,
                               bool is_horizontal,
                               CanvasRotationInVertical rotation,
                               const SimpleFontData* font_data) {
            auto* collector = static_cast<GlyphCollector*>(context);
            collector->glyphs.push_back(glyph);
            collector->positions.push_back(total_advance);
          }
        };
        GlyphCollector collector;
        shape->ForEachGlyph(0, GlyphCollector::Callback, &collector);

        // Serialize glyph array
        auto glyphs_array = std::make_unique<JSONArray>();
        for (uint16_t g : collector.glyphs) {
          glyphs_array->PushInteger(g);
        }
        run_json->SetArray("glyphs", std::move(glyphs_array));

        // Serialize positions array
        auto pos_array = std::make_unique<JSONArray>();
        for (float p : collector.positions) {
          pos_array->PushDouble(p);
        }
        run_json->SetArray("positions", std::move(pos_array));

        // Positioning type (1 = horizontal)
        run_json->SetInteger("positioning", 1);

        runs_array->PushObject(std::move(run_json));
        frag_json->SetArray("runs", std::move(runs_array));
      }

      fragments_array->PushObject(std::move(frag_json));
    }

    json->SetArray("fragments", std::move(fragments_array));
  }

  // === FULL BOX MODEL DATA ===
  if (object.IsBox()) {
    const auto& box = To<LayoutBox>(object);
    const ComputedStyle& style = object.StyleRef();

    // 1. GEOMETRY (absolute coordinates)
    auto geometry = std::make_unique<JSONObject>();
    PhysicalRect border_box = box.PhysicalBorderBoxRect();
    // Transform to absolute document coordinates
    PhysicalOffset offset = box.LocalToAbsolutePoint(PhysicalOffset());
    geometry->SetDouble("x", (border_box.X() + offset.left).ToDouble());
    geometry->SetDouble("y", (border_box.Y() + offset.top).ToDouble());
    geometry->SetDouble("width", border_box.Width().ToDouble());
    geometry->SetDouble("height", border_box.Height().ToDouble());
    json->SetObject("geometry", std::move(geometry));

    // 2. BORDER RADII (8 values) - using FloatValueForLength to resolve Length values
    if (style.HasBorderRadius()) {
      auto radii_array = std::make_unique<JSONArray>();
      // Use border box width/height as reference for percentage values
      float ref_width = border_box.Width().ToFloat();
      float ref_height = border_box.Height().ToFloat();
      // Top-left
      radii_array->PushDouble(FloatValueForLength(style.BorderTopLeftRadius().Width(), ref_width));
      radii_array->PushDouble(FloatValueForLength(style.BorderTopLeftRadius().Height(), ref_height));
      // Top-right
      radii_array->PushDouble(FloatValueForLength(style.BorderTopRightRadius().Width(), ref_width));
      radii_array->PushDouble(FloatValueForLength(style.BorderTopRightRadius().Height(), ref_height));
      // Bottom-right
      radii_array->PushDouble(FloatValueForLength(style.BorderBottomRightRadius().Width(), ref_width));
      radii_array->PushDouble(FloatValueForLength(style.BorderBottomRightRadius().Height(), ref_height));
      // Bottom-left
      radii_array->PushDouble(FloatValueForLength(style.BorderBottomLeftRadius().Width(), ref_width));
      radii_array->PushDouble(FloatValueForLength(style.BorderBottomLeftRadius().Height(), ref_height));
      json->SetArray("border_radii", std::move(radii_array));
    }

    // 3. BORDER WIDTHS (returns int, not Length)
    if (style.HasBorder()) {
      auto border = std::make_unique<JSONObject>();
      border->SetInteger("top", style.BorderTopWidth());
      border->SetInteger("right", style.BorderRightWidth());
      border->SetInteger("bottom", style.BorderBottomWidth());
      border->SetInteger("left", style.BorderLeftWidth());
      json->SetObject("border_widths", std::move(border));

      // 3b. BORDER COLORS (using Longhand references)
      auto border_colors = std::make_unique<JSONObject>();
      auto serialize_border_color = [&style](const Longhand& property) {
        Color c = style.VisitedDependentColor(property);
        auto obj = std::make_unique<JSONObject>();
        obj->SetDouble("r", c.Red() / 255.0);
        obj->SetDouble("g", c.Green() / 255.0);
        obj->SetDouble("b", c.Blue() / 255.0);
        obj->SetDouble("a", c.Alpha());
        return obj;
      };
      border_colors->SetObject("top", serialize_border_color(GetCSSPropertyBorderTopColor()));
      border_colors->SetObject("right", serialize_border_color(GetCSSPropertyBorderRightColor()));
      border_colors->SetObject("bottom", serialize_border_color(GetCSSPropertyBorderBottomColor()));
      border_colors->SetObject("left", serialize_border_color(GetCSSPropertyBorderLeftColor()));
      json->SetObject("border_colors", std::move(border_colors));
    }

    // 4. PADDING
    auto padding = std::make_unique<JSONObject>();
    padding->SetDouble("top", box.PaddingTop().ToDouble());
    padding->SetDouble("right", box.PaddingRight().ToDouble());
    padding->SetDouble("bottom", box.PaddingBottom().ToDouble());
    padding->SetDouble("left", box.PaddingLeft().ToDouble());
    json->SetObject("padding", std::move(padding));

    // 5. MARGIN
    auto margin = std::make_unique<JSONObject>();
    margin->SetDouble("top", box.MarginTop().ToDouble());
    margin->SetDouble("right", box.MarginRight().ToDouble());
    margin->SetDouble("bottom", box.MarginBottom().ToDouble());
    margin->SetDouble("left", box.MarginLeft().ToDouble());
    json->SetObject("margin", std::move(margin));

    // 6. BACKGROUND COLOR
    if (style.HasBackground()) {
      Color bg = style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
      auto bg_color = std::make_unique<JSONObject>();
      bg_color->SetDouble("r", bg.Red() / 255.0);
      bg_color->SetDouble("g", bg.Green() / 255.0);
      bg_color->SetDouble("b", bg.Blue() / 255.0);
      bg_color->SetDouble("a", bg.Alpha());
      json->SetObject("background_color", std::move(bg_color));
    }

    // 7. BOX SHADOW (BoxShadow() returns ShadowList*, iterate via Shadows())
    if (style.BoxShadow()) {
      auto shadows_array = std::make_unique<JSONArray>();
      for (const ShadowData& shadow : style.BoxShadow()->Shadows()) {
        auto shadow_json = std::make_unique<JSONObject>();
        shadow_json->SetDouble("offset_x", shadow.X());
        shadow_json->SetDouble("offset_y", shadow.Y());
        shadow_json->SetDouble("blur", shadow.BlurRadius());
        shadow_json->SetDouble("spread", shadow.Spread());
        shadow_json->SetBoolean("inset", shadow.Style() == ShadowStyle::kInset);
        // Get color - use GetColor() if it's an absolute color (not currentColor)
        const StyleColor& style_color = shadow.GetColor();
        Color c;
        if (style_color.IsAbsoluteColor()) {
          c = style_color.GetColor();
        } else {
          // Fallback: resolve using the element's color property
          c = style.VisitedDependentColor(GetCSSPropertyColor());
        }
        auto color = std::make_unique<JSONObject>();
        color->SetDouble("r", c.Red() / 255.0);
        color->SetDouble("g", c.Green() / 255.0);
        color->SetDouble("b", c.Blue() / 255.0);
        color->SetDouble("a", c.Alpha());
        shadow_json->SetObject("color", std::move(color));
        shadows_array->PushObject(std::move(shadow_json));
      }
      json->SetArray("box_shadow", std::move(shadows_array));
    }
  }

  return json;
}

// First pass: Assign unique IDs to all LayoutObjects
// Using uintptr_t keys to avoid blink-gc plugin restrictions
void AssignLayoutObjectIds(
    const LayoutObject& object,
    std::unordered_map<uintptr_t, int>& object_to_id) {
  int my_id = g_layout_object_id_counter++;
  object_to_id[reinterpret_cast<uintptr_t>(&object)] = my_id;

  for (LayoutObject* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    AssignLayoutObjectIds(*child, object_to_id);
  }
}

// Second pass: Log the LAYOUT TREE structure recursively (DOM order)
void LogLayoutTree(
    const LayoutObject& object,
    JSONArray* objects_array,
    const std::unordered_map<uintptr_t, int>& object_to_id,
    int depth = 0) {
  auto it = object_to_id.find(reinterpret_cast<uintptr_t>(&object));
  int my_id = (it != object_to_id.end()) ? it->second : -1;

  auto obj_json = SerializeLayoutObject(object, my_id);
  obj_json->SetInteger("depth", depth);

  // Add children IDs in DOM order
  auto children_array = std::make_unique<JSONArray>();
  for (LayoutObject* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    auto child_it = object_to_id.find(reinterpret_cast<uintptr_t>(child));
    if (child_it != object_to_id.end()) {
      children_array->PushInteger(child_it->second);
    }
  }
  obj_json->SetArray("children", std::move(children_array));

  objects_array->PushObject(std::move(obj_json));

  // Recurse to children in DOM order
  for (LayoutObject* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    LogLayoutTree(*child, objects_array, object_to_id, depth + 1);
  }
}

// First pass: Assign unique IDs to all PaintLayers
// Using uintptr_t keys to avoid blink-gc plugin restrictions
void AssignPaintLayerIds(
    const PaintLayer& layer,
    std::unordered_map<uintptr_t, int>& layer_to_id) {
  int my_id = g_paint_layer_id_counter++;
  layer_to_id[reinterpret_cast<uintptr_t>(&layer)] = my_id;

  for (PaintLayer* child = layer.FirstChild(); child;
       child = child->NextSibling()) {
    AssignPaintLayerIds(*child, layer_to_id);
  }
}

// Serialize a single PaintLayer to JSON with unique ID
std::unique_ptr<JSONObject> SerializePaintLayer(
    const PaintLayer& layer,
    int id,
    const std::unordered_map<uintptr_t, int>& layer_to_id) {
  auto json = std::make_unique<JSONObject>();
  json->SetInteger("id", id);
  json->SetString("name", GetLayoutObjectDebugName(layer.GetLayoutObject()));
  json->SetInteger("z_index",
                   layer.GetLayoutObject().StyleRef().EffectiveZIndex());
  json->SetBoolean("is_stacking_context",
                   layer.GetLayoutObject().IsStackingContext());
  json->SetBoolean("is_stacked", layer.GetLayoutObject().IsStacked());
  json->SetBoolean("is_self_painting", layer.IsSelfPaintingLayer());

  // Add parent ID
  if (layer.Parent()) {
    auto parent_it = layer_to_id.find(reinterpret_cast<uintptr_t>(layer.Parent()));
    if (parent_it != layer_to_id.end()) {
      json->SetInteger("parent_id", parent_it->second);
    }
  }

  // Add children IDs in paint layer tree order
  auto children_array = std::make_unique<JSONArray>();
  for (PaintLayer* child = layer.FirstChild(); child;
       child = child->NextSibling()) {
    auto child_it = layer_to_id.find(reinterpret_cast<uintptr_t>(child));
    if (child_it != layer_to_id.end()) {
      children_array->PushInteger(child_it->second);
    }
  }
  json->SetArray("children", std::move(children_array));

  return json;
}

// Second pass: Log the PaintLayer tree structure recursively
void LogPaintLayerTree(
    const PaintLayer& layer,
    JSONArray* layers_array,
    const std::unordered_map<uintptr_t, int>& layer_to_id,
    int depth = 0) {
  auto it = layer_to_id.find(reinterpret_cast<uintptr_t>(&layer));
  int my_id = (it != layer_to_id.end()) ? it->second : -1;

  auto layer_json = SerializePaintLayer(layer, my_id, layer_to_id);
  layer_json->SetInteger("depth", depth);
  layers_array->PushObject(std::move(layer_json));

  // Recurse to children in paint layer tree order
  for (PaintLayer* child = layer.FirstChild(); child;
       child = child->NextSibling()) {
    LogPaintLayerTree(*child, layers_array, layer_to_id, depth + 1);
  }
}

// Serialize a PaintLayerStackingNode to JSON - shows pos/neg z-order lists
// Takes stacking_node directly since StackingNode() is private
std::unique_ptr<JSONObject> SerializeStackingNode(
    const PaintLayer& layer,
    int layer_id,
    PaintLayerStackingNode* stacking_node,
    const std::unordered_map<uintptr_t, int>& layer_to_id) {
  auto json = std::make_unique<JSONObject>();
  json->SetInteger("layer_id", layer_id);
  json->SetString("layer", GetLayoutObjectDebugName(layer.GetLayoutObject()));

  if (!stacking_node) {
    json->SetBoolean("has_stacking_node", false);
    return json;
  }

  json->SetBoolean("has_stacking_node", true);

  // Serialize negative z-order list with IDs
  auto neg_list = std::make_unique<JSONArray>();
  for (const auto& child : stacking_node->NegZOrderList()) {
    auto child_json = std::make_unique<JSONObject>();
    auto child_it = layer_to_id.find(reinterpret_cast<uintptr_t>(child.Get()));
    if (child_it != layer_to_id.end()) {
      child_json->SetInteger("id", child_it->second);
    }
    child_json->SetString("name",
                          GetLayoutObjectDebugName(child->GetLayoutObject()));
    child_json->SetInteger(
        "z_index", child->GetLayoutObject().StyleRef().EffectiveZIndex());
    neg_list->PushObject(std::move(child_json));
  }
  json->SetArray("neg_z_order_list", std::move(neg_list));

  // Serialize positive z-order list with IDs
  auto pos_list = std::make_unique<JSONArray>();
  for (const auto& child : stacking_node->PosZOrderList()) {
    auto child_json = std::make_unique<JSONObject>();
    auto child_it = layer_to_id.find(reinterpret_cast<uintptr_t>(child.Get()));
    if (child_it != layer_to_id.end()) {
      child_json->SetInteger("id", child_it->second);
    }
    child_json->SetString("name",
                          GetLayoutObjectDebugName(child->GetLayoutObject()));
    child_json->SetInteger(
        "z_index", child->GetLayoutObject().StyleRef().EffectiveZIndex());
    pos_list->PushObject(std::move(child_json));
  }
  json->SetArray("pos_z_order_list", std::move(pos_list));

  return json;
}

}  // namespace

bool PaintLayerPainter::PaintedOutputInvisible(const ComputedStyle& style) {
  if (style.HasNonInitialBackdropFilter()) {
    return false;
  }

  // Always paint when 'will-change: opacity' is present. Reduces jank for
  // common animation implementation approaches, for example, an element that
  // starts with opacity zero and later begins to animate.
  if (style.HasWillChangeOpacityHint()) {
    return false;
  }

  if (style.HasCurrentOpacityAnimation()) {
    return false;
  }

  // 0.0004f < 1/2048. With 10-bit color channels (only available on the
  // newest Macs; otherwise it's 8-bit), we see that an alpha of 1/2048 or
  // less leads to a color output of less than 0.5 in all channels, hence
  // not visible.
  static const float kMinimumVisibleOpacity = 0.0004f;
  if (style.Opacity() < kMinimumVisibleOpacity) {
    return true;
  }

  return false;
}

PhysicalRect PaintLayerPainter::ContentsVisualRect(const FragmentData& fragment,
                                                   const LayoutBox& box) {
  PhysicalRect contents_visual_rect = box.ContentsVisualOverflowRect();
  contents_visual_rect.Move(fragment.PaintOffset());
  const auto* replaced_transform =
      fragment.PaintProperties()
          ? fragment.PaintProperties()->ReplacedContentTransform()
          : nullptr;
  if (replaced_transform) {
    gfx::RectF float_contents_visual_rect(contents_visual_rect);
    GeometryMapper::SourceToDestinationRect(*replaced_transform->Parent(),
                                            *replaced_transform,
                                            float_contents_visual_rect);
    contents_visual_rect =
        PhysicalRect::EnclosingRect(float_contents_visual_rect);
  }
  return contents_visual_rect;
}

static gfx::Rect FirstFragmentVisualRect(const LayoutBoxModelObject& object) {
  // We don't want to include overflowing contents.
  PhysicalRect overflow_rect =
      object.IsBox() ? To<LayoutBox>(object).SelfVisualOverflowRect()
                     : object.VisualOverflowRect();
  overflow_rect = object.ApplyFiltersToRect(overflow_rect);
  overflow_rect.Move(object.FirstFragment().PaintOffset());
  return ToEnclosingRect(overflow_rect);
}

static bool ShouldCreateSubsequence(const PaintLayer& paint_layer,
                                    const GraphicsContext& context,
                                    PaintFlags paint_flags) {
  // Caching is not needed during printing or painting previews.
  if (paint_layer.GetLayoutObject()
          .GetDocument()
          .IsPrintingOrPaintingPreview()) {
    return false;
  }

  if (context.GetPaintController().IsSkippingCache()) {
    return false;
  }

  if (!paint_layer.SupportsSubsequenceCaching()) {
    return false;
  }

  // Don't create subsequence during special painting to avoid cache conflict
  // with normal painting.
  if (paint_flags & PaintFlag::kOmitCompositingInfo) {
    return false;
  }

  // Create subsequence if the layer will create a paint chunk because of
  // different properties.
  if (context.GetPaintController().NumNewChunks() > 0 &&
      paint_layer.GetLayoutObject()
              .FirstFragment()
              .LocalBorderBoxProperties() !=
          context.GetPaintController().LastChunkProperties()) {
    return true;
  }

  // Create subsequence if the layer has at least 2 descendants,
  if (paint_layer.FirstChild() && (paint_layer.FirstChild()->FirstChild() ||
                                   paint_layer.FirstChild()->NextSibling())) {
    return true;
  }

  if (context.GetPaintController().NumNewChunks()) {
    const auto& object = paint_layer.GetLayoutObject();

    // Or if merged hit test opaqueness would become kMixed if either the
    // current chunk or this layer is transparent to hit test, for better
    // compositor hit test performance.
    bool transparent_to_hit_test =
        ObjectPainter(object).GetHitTestOpaqueness() ==
        cc::HitTestOpaqueness::kTransparent;
    if (transparent_to_hit_test !=
        context.GetPaintController()
            .CurrentChunkIsNonEmptyAndTransparentToHitTest()) {
      return true;
    }

    // Or if the merged bounds with the last chunk would be too empty.
    gfx::Rect last_bounds = context.GetPaintController().LastChunkBounds();
    gfx::Rect visual_rect = FirstFragmentVisualRect(object);
    gfx::Rect merged_bounds = gfx::UnionRects(last_bounds, visual_rect);
    float device_pixel_ratio =
        object.GetFrame()->LocalFrameRoot().GetDocument()->DevicePixelRatio();
    // This is similar to the condition in PendingLayer::CanMerge().
    if (merged_bounds.size().Area64() >
        10000 * device_pixel_ratio * device_pixel_ratio +
            last_bounds.size().Area64() + visual_rect.size().Area64()) {
      return true;
    }
  }

  return false;
}

PaintResult PaintLayerPainter::Paint(GraphicsContext& context,
                                     PaintFlags paint_flags) {
  const auto& object = paint_layer_.GetLayoutObject();
  if (object.NeedsLayout() && !object.ChildLayoutBlockedByDisplayLock())
      [[unlikely]] {
    // Skip if we need layout. This should never happen. See crbug.com/1423308
    // and crbug.com/330051489.
    return kFullyPainted;
  }

  if (object.GetFrameView()->ShouldThrottleRendering()) {
    return kFullyPainted;
  }

  if (object.IsFragmentLessBox()) {
    return kFullyPainted;
  }

  // Non self-painting layers without self-painting descendants don't need to be
  // painted as their layoutObject() should properly paint itself.
  if (!paint_layer_.IsSelfPaintingLayer() &&
      !paint_layer_.HasSelfPaintingLayerDescendant()) {
    return kFullyPainted;
  }

  std::optional<CheckAncestorPositionVisibilityScope>
      check_position_visibility_scope;
  if (paint_layer_.InvisibleForPositionVisibility() ||
      paint_layer_.HasAncestorInvisibleForPositionVisibility()) {
    return kFullyPainted;
  }
  if (paint_layer_.GetLayoutObject().IsStackingContext()) {
    check_position_visibility_scope.emplace(paint_layer_);
  }

  // A paint layer should always have LocalBorderBoxProperties when it's ready
  // for paint.
  if (!object.FirstFragment().HasLocalBorderBoxProperties()) {
    // TODO(crbug.com/848056): This can happen e.g. when we paint a filter
    // referencing a SVG foreign object through feImage, especially when there
    // is circular references. Should find a better solution.
    return kMayBeClippedByCullRect;
  }

  // Log the full LAYOUT TREE structure at the root (DOM order)
  if (IsA<LayoutView>(object)) {
    // Reset ID counters for this logging pass
    g_layout_object_id_counter = 0;
    g_paint_layer_id_counter = 0;

    // First pass: assign IDs to all LayoutObjects
    std::unordered_map<uintptr_t, int> object_to_id;
    AssignLayoutObjectIds(object, object_to_id);

    // Second pass: log layout tree with IDs
    auto tree_json = std::make_unique<JSONObject>();
    auto layout_tree_array = std::make_unique<JSONArray>();
    LogLayoutTree(object, layout_tree_array.get(), object_to_id);
    tree_json->SetArray("layout_tree", std::move(layout_tree_array));
    LOG(ERROR) << "LAYOUT_TREE: " << tree_json->ToJSONString().Utf8();

    // First pass: assign IDs to all PaintLayers
    std::unordered_map<uintptr_t, int> layer_to_id;
    AssignPaintLayerIds(paint_layer_, layer_to_id);

    // Second pass: log PaintLayer tree with IDs
    auto layer_tree_json = std::make_unique<JSONObject>();
    auto layer_tree_array = std::make_unique<JSONArray>();
    LogPaintLayerTree(paint_layer_, layer_tree_array.get(), layer_to_id);
    layer_tree_json->SetArray("layer_tree", std::move(layer_tree_array));
    LOG(ERROR) << "LAYER_TREE: " << layer_tree_json->ToJSONString().Utf8();

    // Log all stacking nodes (z-order lists) for paint order info
    // We collect inline here since we have friend access to StackingNode()
    auto paint_order_json = std::make_unique<JSONObject>();
    auto stacking_nodes_array = std::make_unique<JSONArray>();

    // Use a lambda to recursively collect stacking nodes
    std::function<void(const PaintLayer&)> collect_stacking_nodes =
        [&](const PaintLayer& layer) {
          PaintLayerStackingNode* stacking_node = layer.StackingNode();
          if (stacking_node) {
            auto it = layer_to_id.find(reinterpret_cast<uintptr_t>(&layer));
            int layer_id = (it != layer_to_id.end()) ? it->second : -1;
            stacking_nodes_array->PushObject(
                SerializeStackingNode(layer, layer_id, stacking_node,
                                      layer_to_id));
          }
          for (PaintLayer* child = layer.FirstChild(); child;
               child = child->NextSibling()) {
            collect_stacking_nodes(*child);
          }
        };
    collect_stacking_nodes(paint_layer_);

    paint_order_json->SetArray("stacking_nodes", std::move(stacking_nodes_array));
    LOG(ERROR) << "PAINT_ORDER: " << paint_order_json->ToJSONString().Utf8();

    // Log Property Trees
    PropertyTreeIdMapper property_tree_mapper;
    std::function<void(const LayoutObject&)> collect_property_tree_nodes =
        [&](const LayoutObject& object) {
          for (const FragmentData& fragment : FragmentDataIterator(object)) {
            if (fragment.HasLocalBorderBoxProperties()) {
              PropertyTreeStateOrAlias state =
                  fragment.LocalBorderBoxProperties();
              property_tree_mapper.GetOrAssignTransformId(
                  &state.Transform().Unalias());
              property_tree_mapper.GetOrAssignClipId(&state.Clip().Unalias());
              property_tree_mapper.GetOrAssignEffectId(
                  &state.Effect().Unalias());
            }
          }
          for (const LayoutObject* child = object.SlowFirstChild(); child;
               child = child->NextSibling()) {
            collect_property_tree_nodes(*child);
          }
        };

    // Retrieve LayoutView via the current paint_layer's LayoutObject
    collect_property_tree_nodes(*paint_layer_.GetLayoutObject().View());

    auto property_trees_json = std::make_unique<JSONObject>();
    property_trees_json->SetObject(
        "transform_tree",
        SerializeTransformTree(property_tree_mapper.GetTransformIds()));
    property_trees_json->SetObject(
        "clip_tree", SerializeClipTree(property_tree_mapper.GetClipIds()));
    property_trees_json->SetObject(
        "effect_tree",
        SerializeEffectTree(property_tree_mapper.GetEffectIds()));

    LOG(ERROR) << "PROPERTY_TREES: "
               << property_trees_json->ToJSONString().Utf8();
  }

  bool selection_drag_image_only =
      paint_flags & PaintFlag::kSelectionDragImageOnly;
  if (selection_drag_image_only && !object.IsSelected()) {
    return kFullyPainted;
  }

  IgnorePaintTimingScope ignore_paint_timing;
  if (object.StyleRef().Opacity() == 0.0f) {
    IgnorePaintTimingScope::IncrementIgnoreDepth();
  }
  // Explicitly compute opacity of documentElement, as it is special-cased in
  // Largest Contentful Paint.
  bool is_document_element_invisible = false;
  if (const auto* document_element = object.GetDocument().documentElement()) {
    if (document_element->GetLayoutObject() &&
        document_element->GetLayoutObject()->StyleRef().Opacity() == 0.0f) {
      is_document_element_invisible = true;
    }
  }
  IgnorePaintTimingScope::SetIsDocumentElementInvisible(
      is_document_element_invisible);

  bool is_self_painting_layer = paint_layer_.IsSelfPaintingLayer();
  bool should_paint_content =
      paint_layer_.HasVisibleContent() &&
      // Content under a LayoutSVGHiddenContainer is auxiliary resources for
      // painting. Foreign content should never paint in this situation, as it
      // is primary, not auxiliary.
      !paint_layer_.IsUnderSVGHiddenContainer() && is_self_painting_layer;

  PaintResult result = kFullyPainted;
  if (object.IsFragmented() ||
      // When printing, the LayoutView's background should extend infinitely
      // regardless of LayoutView's visual rect, so don't check intersection
      // between the visual rect and the cull rect (custom for each page).
      (IsA<LayoutView>(object) && object.GetDocument().Printing())) {
    result = kMayBeClippedByCullRect;
  } else {
    gfx::Rect visual_rect = FirstFragmentVisualRect(object);
    gfx::Rect cull_rect = object.FirstFragment().GetCullRect().Rect();
    bool cull_rect_intersects_self = cull_rect.Intersects(visual_rect);
    if (!cull_rect.Contains(visual_rect)) {
      result = kMayBeClippedByCullRect;
    }

    bool cull_rect_intersects_contents = true;
    if (const auto* box = DynamicTo<LayoutBox>(object)) {
      PhysicalRect contents_visual_rect(
          ContentsVisualRect(object.FirstFragment(), *box));
      PhysicalRect contents_cull_rect(
          object.FirstFragment().GetContentsCullRect().Rect());
      cull_rect_intersects_contents =
          contents_cull_rect.Intersects(contents_visual_rect);
      if (!contents_cull_rect.Contains(contents_visual_rect)) {
        result = kMayBeClippedByCullRect;
      }
    } else {
      cull_rect_intersects_contents = cull_rect_intersects_self;
    }

    if (!cull_rect_intersects_self && !cull_rect_intersects_contents) {
      if (paint_layer_.KnownToClipSubtreeToPaddingBox()) {
        paint_layer_.SetPreviousPaintResult(kMayBeClippedByCullRect);
        return kMayBeClippedByCullRect;
      }
      should_paint_content = false;
    }

    // The above doesn't consider clips on non-self-painting contents.
    // Will update in ScopedBoxContentsPaintState.
  }

  bool should_create_subsequence =
      should_paint_content &&
      ShouldCreateSubsequence(paint_layer_, context, paint_flags);
  std::optional<SubsequenceRecorder> subsequence_recorder;
  if (should_create_subsequence) {
    if (!paint_layer_.SelfOrDescendantNeedsRepaint() &&
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context,
                                                            paint_layer_)) {
      return paint_layer_.PreviousPaintResult();
    }
    DCHECK(paint_layer_.SupportsSubsequenceCaching());
    subsequence_recorder.emplace(context, paint_layer_);
  }

  PaintTimelineReporter timeline_reporter(paint_layer_, should_paint_content);
  PaintController& controller = context.GetPaintController();

  std::optional<ScopedEffectivelyInvisible> effectively_invisible;
  if (PaintedOutputInvisible(object.StyleRef())) {
    effectively_invisible.emplace(controller);
  }

  std::optional<ScopedPaintChunkProperties> layer_chunk_properties;

  // The parent effect (before creating layer_chunk_properties for the current
  // object) is used to paint the foreign layer for the transition scope
  // snapshot. This is because the scope's paint properties includes the effect
  // that is capturing the scope snapshot. The foreign layer is the destination
  // of that capture, so painting it inside the effect would be circular.
  const EffectPaintPropertyNodeOrAlias* parent_effect = nullptr;
  {
    auto& parent_props = controller.CurrentPaintChunkProperties();
    if (parent_props.IsInitialized()) {
      parent_effect = &parent_props.Effect();
    }
  }

  if (should_paint_content) {
    // If we will create a new paint chunk for this layer, this gives the chunk
    // a stable id.
    layer_chunk_properties.emplace(
        controller, object.FirstFragment().LocalBorderBoxProperties(),
        paint_layer_, DisplayItem::kLayerChunk);

    // When a reference filter applies to the layer, ensure a chunk is
    // generated so that the filter paints even if no other content is painted
    // by the layer (see `SVGContainerPainter::Paint`).
    auto* properties = object.FirstFragment().PaintProperties();
    if (properties && properties->Filter() &&
        properties->Filter()->HasReferenceFilter()) {
      controller.EnsureChunk();
    }
  }

  bool should_paint_background =
      should_paint_content && !selection_drag_image_only;
  if (should_paint_background) {
    PaintWithPhase(PaintPhase::kSelfBlockBackgroundOnly, context, paint_flags);
  }

  if (PaintChildren(kNegativeZOrderChildren, context, paint_flags) ==
      kMayBeClippedByCullRect) {
    result = kMayBeClippedByCullRect;
  }

  if (should_paint_content) {
    // If the negative-z-order children created paint chunks, this gives the
    // foreground paint chunk a stable id.
    ScopedPaintChunkProperties foreground_properties(
        controller, object.FirstFragment().LocalBorderBoxProperties(),
        paint_layer_, DisplayItem::kLayerChunkForeground);

    if (selection_drag_image_only) {
      PaintWithPhase(PaintPhase::kSelectionDragImage, context, paint_flags);
    } else {
      PaintForegroundPhases(context, paint_flags);
    }
  }

  // Outline always needs to be painted even if we have no visible content.
  bool should_paint_self_outline =
      is_self_painting_layer && object.StyleRef().HasOutline();

  bool is_video = IsA<LayoutVideo>(object);
  if (!is_video && should_paint_self_outline) {
    PaintWithPhase(PaintPhase::kSelfOutlineOnly, context, paint_flags);
  }

  if (PaintChildren(kNormalFlowAndPositiveZOrderChildren, context,
                    paint_flags) == kMayBeClippedByCullRect) {
    result = kMayBeClippedByCullRect;
  }

  if (should_paint_content && paint_layer_.GetScrollableArea() &&
      paint_layer_.GetScrollableArea()
          ->ShouldOverflowControlsPaintAsOverlay()) {
    if (!paint_layer_.NeedsReorderOverlayOverflowControls()) {
      PaintOverlayOverflowControls(context, paint_flags);
    }
    // Otherwise the overlay overflow controls will be painted after scrolling
    // children in PaintChildren().
  }
  // Overlay overflow controls of scrollers without a self-painting layer are
  // painted in the foreground paint phase. See ScrollableAreaPainter.

  if (is_video && should_paint_self_outline) {
    // We paint outlines for video later so that they aren't obscured by the
    // video controls.
    PaintWithPhase(PaintPhase::kSelfOutlineOnly, context, paint_flags);
  }

  if (const auto* properties = object.FirstFragment().PaintProperties()) {
    if (should_paint_content && !selection_drag_image_only) {
      if (properties->Mask()) {
        if (object.IsSVGForeignObject()) {
          SVGMaskPainter::Paint(context, object, object);
        } else {
          PaintWithPhase(PaintPhase::kMask, context, paint_flags);
        }
      }
      if (properties->ClipPathMask()) {
        ClipPathClipper::PaintClipPathAsMaskImage(context, object, object);
      }
    }
    if (PaintTransitionPseudos(context, object, paint_flags) ==
        kMayBeClippedByCullRect) {
      result = kMayBeClippedByCullRect;
    }
    if (should_paint_content && !selection_drag_image_only) {
      PaintTransitionScopeSnapshotIfNeeded(context, object, parent_effect);
    }
  }

  paint_layer_.SetPreviousPaintResult(result);
  return result;
}

void PaintLayerPainter::PaintTransitionScopeSnapshotIfNeeded(
    GraphicsContext& context,
    const LayoutBoxModelObject& object,
    const EffectPaintPropertyNodeOrAlias* effect) {
  auto& controller = context.GetPaintController();
  auto layer = GetTransitionScopeSnapshotLayer(object.GetNode());
  if (!layer) {
    return;
  }

  PhysicalRect box_border_rect =
      paint_layer_.LocalBoundingBoxIncludingSelfPaintingDescendants();
  PhysicalRect ink_overflow_rect = object.ApplyFiltersToRect(box_border_rect);
  PhysicalOffset paint_offset = ink_overflow_rect.offset;
  layer->SetBounds(ink_overflow_rect.PixelSnappedSize());
  layer->SetIsDrawable(true);

  PropertyTreeStateOrAlias properties =
      controller.CurrentPaintChunkProperties();
  DCHECK(effect);
  properties.SetEffect(*effect);

  RecordForeignLayer(
      context, paint_layer_, DisplayItem::kForeignLayerViewTransitionContent,
      std::move(layer), ToRoundedPoint(paint_offset), &properties);
}

PaintResult PaintLayerPainter::PaintTransitionPseudos(
    GraphicsContext& context,
    const LayoutBoxModelObject& object,
    PaintFlags paint_flags) {
  const Element* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    return kFullyPainted;
  }
  LayoutBoxModelObject* pseudo_layout_object = DynamicTo<LayoutBoxModelObject>(
      element->PseudoElementLayoutObject(kPseudoIdViewTransition));
  if (!pseudo_layout_object) {
    return kFullyPainted;
  }
  auto* transition = ViewTransitionUtils::GetTransition(*element);
  if (!transition || transition->IsCapturing()) {
    // Don't paint the pseudos during the capture phase. This avoids scaling
    // problems in the scope snapshot layer when participants overflow.
    // Note: PaintTransitionScopeSnapshotIfNeeded will paint the scope snapshot
    // layer during capture, ensuring that the scope remains visible.
    return kFullyPainted;
  }
  PaintLayer* pseudo_layer = pseudo_layout_object->Layer();
  if (!pseudo_layer || pseudo_layer->Parent() != &paint_layer_) {
    return kFullyPainted;
  }
  return PaintLayerPainter(*pseudo_layer).Paint(context, paint_flags);
}

PaintResult PaintLayerPainter::PaintChildren(
    PaintLayerIteration children_to_visit,
    GraphicsContext& context,
    PaintFlags paint_flags) {
  PaintResult result = kFullyPainted;
  if (!paint_layer_.HasSelfPaintingLayerDescendant()) {
    return result;
  }

  LayoutObject& layout_object = paint_layer_.GetLayoutObject();
  if (layout_object.ChildPaintBlockedByDisplayLock()) {
    return result;
  }

  // Prevent canvas fallback content from being rendered.
  if (IsA<HTMLCanvasElement>(layout_object.GetNode())) {
    return result;
  }

  PaintLayerPaintOrderIterator iterator(&paint_layer_, children_to_visit);
  while (PaintLayer* child = iterator.Next()) {
    if (child->IsReplacedNormalFlowStacking()) {
      continue;
    }

    if (!layout_object.IsViewTransitionRoot() &&
        ViewTransitionUtils::IsViewTransitionRoot(child->GetLayoutObject())) {
      // Skip scoped ::view-transition pseudo, which is painted separately by
      // PaintTransitionPseudos so that it is top of all other children of the
      // scope regardless of their z-index.
      continue;
    }

    if (PaintLayerPainter(*child).Paint(context, paint_flags) ==
        kMayBeClippedByCullRect) {
      result = kMayBeClippedByCullRect;
    }

    if (const auto* layers_painting_overlay_overflow_controls_after =
            iterator.LayersPaintingOverlayOverflowControlsAfter(child)) {
      for (auto& reparent_overflow_controls_layer :
           *layers_painting_overlay_overflow_controls_after) {
        DCHECK(reparent_overflow_controls_layer
                   ->NeedsReorderOverlayOverflowControls());
        PaintLayerPainter(*reparent_overflow_controls_layer)
            .PaintOverlayOverflowControls(context, paint_flags);
        if (reparent_overflow_controls_layer->PreviousPaintResult() ==
            kMayBeClippedByCullRect) {
          result = kMayBeClippedByCullRect;
        }
      }
    }
  }

  return result;
}

void PaintLayerPainter::PaintOverlayOverflowControls(GraphicsContext& context,
                                                     PaintFlags paint_flags) {
  DCHECK(paint_layer_.GetScrollableArea());
  DCHECK(
      paint_layer_.GetScrollableArea()->ShouldOverflowControlsPaintAsOverlay());
  PaintWithPhase(PaintPhase::kOverlayOverflowControls, context, paint_flags);
}

void PaintLayerPainter::PaintFragmentWithPhase(
    PaintPhase phase,
    const FragmentData& fragment_data,
    wtf_size_t fragment_data_idx,
    const PhysicalBoxFragment* physical_fragment,
    GraphicsContext& context,
    PaintFlags paint_flags) {
  DCHECK(paint_layer_.IsSelfPaintingLayer() ||
         phase == PaintPhase::kOverlayOverflowControls);

  CullRect cull_rect = fragment_data.GetCullRect();
  if (cull_rect.Rect().IsEmpty()) {
    return;
  }

  auto chunk_properties = fragment_data.LocalBorderBoxProperties();
  if (phase == PaintPhase::kMask) {
    const auto* properties = fragment_data.PaintProperties();
    DCHECK(properties);
    DCHECK(properties->Mask());
    DCHECK(properties->Mask()->OutputClip());
    chunk_properties.SetEffect(*properties->Mask());
    chunk_properties.SetClip(*properties->Mask()->OutputClip());
  }
  ScopedPaintChunkProperties fragment_paint_chunk_properties(
      context.GetPaintController(), chunk_properties, paint_layer_,
      DisplayItem::PaintPhaseToDrawingType(phase));

  PaintInfo paint_info(
      context, cull_rect, phase,
      paint_layer_.GetLayoutObject().ChildPaintBlockedByDisplayLock(),
      paint_flags);

  if (physical_fragment) {
    BoxFragmentPainter(*physical_fragment).Paint(paint_info);
  } else if (const auto* layout_inline =
                 DynamicTo<LayoutInline>(&paint_layer_.GetLayoutObject())) {
    InlineBoxFragmentPainter::PaintAllFragments(*layout_inline, fragment_data,
                                                fragment_data_idx, paint_info);
  } else {
    // We are about to enter legacy paint code. Set the right FragmentData
    // object, to use the right paint offset.
    paint_info.SetFragmentDataOverride(&fragment_data);
    paint_layer_.GetLayoutObject().Paint(paint_info);
  }
}

void PaintLayerPainter::PaintWithPhase(PaintPhase phase,
                                       GraphicsContext& context,
                                       PaintFlags paint_flags) {
  const auto* layout_box_with_fragments =
      paint_layer_.GetLayoutBoxWithBlockFragments();
  wtf_size_t fragment_idx = 0u;

  // The NG paint code guards against painting multiple fragments for content
  // that doesn't support it, but the legacy paint code has no such guards.
  // TODO(crbug.com/1229581): Remove this when everything is handled by NG.
  bool multiple_fragments_allowed =
      layout_box_with_fragments ||
      CanPaintMultipleFragments(paint_layer_.GetLayoutObject());

  for (const FragmentData& fragment :
       FragmentDataIterator(paint_layer_.GetLayoutObject())) {
    const PhysicalBoxFragment* physical_fragment = nullptr;
    if (layout_box_with_fragments) {
      physical_fragment =
          layout_box_with_fragments->GetPhysicalFragment(fragment_idx);
      DCHECK(physical_fragment);
    }

    std::optional<ScopedDisplayItemFragment> scoped_display_item_fragment;
    if (fragment_idx) {
      scoped_display_item_fragment.emplace(context, fragment_idx);
    }

    PaintFragmentWithPhase(phase, fragment, fragment_idx, physical_fragment,
                           context, paint_flags);

    if (!multiple_fragments_allowed) {
      break;
    }

    fragment_idx++;
  }
}

void PaintLayerPainter::PaintForegroundPhases(GraphicsContext& context,
                                              PaintFlags paint_flags) {
  PaintWithPhase(PaintPhase::kDescendantBlockBackgroundsOnly, context,
                 paint_flags);

  if (paint_layer_.GetLayoutObject().GetDocument().InForcedColorsMode()) {
    PaintWithPhase(PaintPhase::kForcedColorsModeBackplate, context,
                   paint_flags);
  }

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
      paint_layer_.NeedsPaintPhaseFloat()) {
    PaintWithPhase(PaintPhase::kFloat, context, paint_flags);
  }

  PaintWithPhase(PaintPhase::kForeground, context, paint_flags);

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
      paint_layer_.NeedsPaintPhaseDescendantOutlines()) {
    PaintWithPhase(PaintPhase::kDescendantOutlinesOnly, context, paint_flags);
  }
}

}  // namespace blink
