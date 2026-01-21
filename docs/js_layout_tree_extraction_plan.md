# Plan: JavaScript-based Layout Tree Extraction

This document outlines how to reproduce the `layout_tree_pre_shape.json` structure using browser JavaScript APIs.

## Target JSON Structure

Each node in the layout tree contains:

```json
{
  "id": number,
  "name": string,           // e.g., "LayoutBlockFlow DIV"
  "tag": string,            // HTML tag (optional, not for text nodes)
  "z_index": number,
  "is_stacking_context": boolean,
  "is_stacked": boolean,
  "has_layer": boolean,
  "is_self_painting": boolean,  // optional
  "computed_style": { ... },
  "geometry": { x, y, width, height },
  "padding": { top, right, bottom, left },
  "margin": { top, right, bottom, left },
  "background_color": { r, g, b, a },       // optional
  "border_radii": [8 values],               // optional
  "border_widths": { top, right, bottom, left },  // optional
  "border_colors": { top, right, bottom, left },  // optional
  "box_shadow": [ ... ],                    // optional
  "text": string,                           // text nodes only
  "fragments": [ ... ],                     // text nodes only
  "depth": number,
  "children": [ids]
}
```

---

## Implementation Plan

### Phase 1: DOM Traversal Infrastructure

**1.1 Create a TreeWalker for full DOM traversal**
```javascript
function createLayoutTreeWalker(root) {
  return document.createTreeWalker(
    root,
    NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT,
    {
      acceptNode: (node) => {
        // Skip empty text nodes (whitespace only)
        if (node.nodeType === Node.TEXT_NODE) {
          return node.textContent.trim() ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_SKIP;
        }
        // Skip <script>, <style>, etc.
        const tag = node.tagName.toLowerCase();
        if (['script', 'style', 'noscript'].includes(tag)) {
          return NodeFilter.FILTER_REJECT;
        }
        return NodeFilter.FILTER_ACCEPT;
      }
    }
  );
}
```

**1.2 Build flat array with parent-child relationships**
- Assign sequential IDs during traversal
- Track depth via recursion level
- Store children as array of IDs (requires two-pass or post-processing)

---

### Phase 2: Geometry Extraction

**2.1 Element geometry via getBoundingClientRect()**
```javascript
function getGeometry(element) {
  const rect = element.getBoundingClientRect();
  return {
    x: rect.x + window.scrollX,
    y: rect.y + window.scrollY,
    width: rect.width,
    height: rect.height
  };
}
```

**2.2 Text node geometry via Range API**
```javascript
function getTextGeometry(textNode) {
  const range = document.createRange();
  range.selectNodeContents(textNode);
  const rects = range.getClientRects();
  // Returns array of DOMRect for each line box
  return Array.from(rects).map(rect => ({
    x: rect.x + window.scrollX,
    y: rect.y + window.scrollY,
    width: rect.width,
    height: rect.height
  }));
}
```

**2.3 Text fragments (for multi-line text)**
```javascript
function getTextFragments(textNode) {
  const range = document.createRange();
  range.selectNodeContents(textNode);
  const rects = range.getClientRects();

  return Array.from(rects).map((rect, i) => ({
    x: rect.x + window.scrollX,
    y: rect.y + window.scrollY,
    width: rect.width,
    height: rect.height,
    start: 0,   // Would need more complex logic to determine character ranges
    end: textNode.textContent.length,
    runs: [{ glyphs: null, positions: null }]  // Not accessible from JS
  }));
}
```

---

### Phase 3: Computed Style Extraction

**3.1 Core computed style properties**
```javascript
function getComputedStyleInfo(element) {
  const style = getComputedStyle(element);

  return {
    display: style.display,
    position: style.position,
    opacity: parseFloat(style.opacity),
    z_index: style.zIndex === 'auto' ? 0 : parseInt(style.zIndex),
    has_transform: style.transform !== 'none',
    overflow_x: style.overflowX,
    overflow_y: style.overflowY,
    visibility: style.visibility,
    has_filter: style.filter !== 'none',
    has_backdrop_filter: style.backdropFilter !== 'none',
    has_clip_path: style.clipPath !== 'none',
    has_mask: style.mask !== 'none' && style.maskImage !== 'none',
    is_isolated: style.isolation === 'isolate',
    has_will_change_transform: style.willChange.includes('transform'),
    has_will_change_opacity: style.willChange.includes('opacity'),
    contain_paint: style.contain.includes('paint'),
    contain_layout: style.contain.includes('layout'),
    letter_spacing: parseFloat(style.letterSpacing) || 0,
    word_spacing: parseFloat(style.wordSpacing) || 0,
    font_size: parseFloat(style.fontSize),
    font_family: style.fontFamily.split(',')[0].replace(/['"]/g, '').trim(),
    font_weight: parseInt(style.fontWeight),
    font_style: style.fontStyle
  };
}
```

**3.2 Text nodes inherit parent's computed style**
```javascript
function getTextNodeStyle(textNode) {
  return getComputedStyleInfo(textNode.parentElement);
}
```

---

### Phase 4: Box Model Properties

**4.1 Padding**
```javascript
function getPadding(element) {
  const style = getComputedStyle(element);
  return {
    top: parseFloat(style.paddingTop),
    right: parseFloat(style.paddingRight),
    bottom: parseFloat(style.paddingBottom),
    left: parseFloat(style.paddingLeft)
  };
}
```

**4.2 Margin**
```javascript
function getMargin(element) {
  const style = getComputedStyle(element);
  return {
    top: parseFloat(style.marginTop),
    right: parseFloat(style.marginRight),
    bottom: parseFloat(style.marginBottom),
    left: parseFloat(style.marginLeft)
  };
}
```

**4.3 Border widths**
```javascript
function getBorderWidths(element) {
  const style = getComputedStyle(element);
  const widths = {
    top: parseFloat(style.borderTopWidth),
    right: parseFloat(style.borderRightWidth),
    bottom: parseFloat(style.borderBottomWidth),
    left: parseFloat(style.borderLeftWidth)
  };
  // Only return if any border exists
  if (widths.top || widths.right || widths.bottom || widths.left) {
    return widths;
  }
  return null;
}
```

**4.4 Border colors (RGBA)**
```javascript
function parseColor(colorStr) {
  // Parse "rgb(r, g, b)" or "rgba(r, g, b, a)"
  const match = colorStr.match(/rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*([\d.]+))?\)/);
  if (match) {
    return {
      r: parseInt(match[1]) / 255,
      g: parseInt(match[2]) / 255,
      b: parseInt(match[3]) / 255,
      a: match[4] !== undefined ? parseFloat(match[4]) : 1
    };
  }
  return null;
}

function getBorderColors(element) {
  const style = getComputedStyle(element);
  return {
    top: parseColor(style.borderTopColor),
    right: parseColor(style.borderRightColor),
    bottom: parseColor(style.borderBottomColor),
    left: parseColor(style.borderLeftColor)
  };
}
```

**4.5 Border radii (8 values: 4 corners x 2 axes)**
```javascript
function getBorderRadii(element) {
  const style = getComputedStyle(element);
  // Each corner has horizontal and vertical radius
  const tl = parseFloat(style.borderTopLeftRadius) || 0;
  const tr = parseFloat(style.borderTopRightRadius) || 0;
  const br = parseFloat(style.borderBottomRightRadius) || 0;
  const bl = parseFloat(style.borderBottomLeftRadius) || 0;

  // Format: [tl-h, tl-v, tr-h, tr-v, br-h, br-v, bl-h, bl-v]
  // For simple radii (single value), h === v
  const radii = [tl, tl, tr, tr, br, br, bl, bl];

  // Only return if any radius exists
  if (radii.some(r => r > 0)) {
    return radii;
  }
  return null;
}
```

---

### Phase 5: Background and Shadows

**5.1 Background color**
```javascript
function getBackgroundColor(element) {
  const style = getComputedStyle(element);
  const color = parseColor(style.backgroundColor);
  // Only return if not transparent
  if (color && color.a > 0) {
    return color;
  }
  return null;
}
```

**5.2 Box shadow parsing**
```javascript
function parseBoxShadow(shadowStr) {
  if (shadowStr === 'none') return null;

  // Box shadow format: [inset?] offset-x offset-y [blur [spread]] color
  const shadows = [];
  // This is a simplified parser - real implementation needs to handle
  // multiple shadows separated by commas

  const regex = /(inset\s+)?(-?[\d.]+)px\s+(-?[\d.]+)px\s+(-?[\d.]+)px(?:\s+(-?[\d.]+)px)?\s+(rgba?\([^)]+\))/g;
  let match;

  while ((match = regex.exec(shadowStr)) !== null) {
    shadows.push({
      inset: !!match[1],
      offset_x: parseFloat(match[2]),
      offset_y: parseFloat(match[3]),
      blur: parseFloat(match[4]),
      spread: match[5] ? parseFloat(match[5]) : 0,
      color: parseColor(match[6])
    });
  }

  return shadows.length > 0 ? shadows : null;
}
```

---

### Phase 6: Stacking Context Detection

**6.1 Determine if element creates stacking context**
```javascript
function createsStackingContext(element) {
  const style = getComputedStyle(element);

  // Root element
  if (element === document.documentElement) return true;

  // Positioned with z-index != auto
  if (style.position !== 'static' && style.zIndex !== 'auto') return true;

  // Flex/Grid children with z-index != auto
  const parent = element.parentElement;
  if (parent) {
    const parentDisplay = getComputedStyle(parent).display;
    if ((parentDisplay === 'flex' || parentDisplay === 'grid') && style.zIndex !== 'auto') {
      return true;
    }
  }

  // opacity < 1
  if (parseFloat(style.opacity) < 1) return true;

  // transform != none
  if (style.transform !== 'none') return true;

  // filter != none
  if (style.filter !== 'none') return true;

  // backdrop-filter != none
  if (style.backdropFilter !== 'none') return true;

  // isolation: isolate
  if (style.isolation === 'isolate') return true;

  // will-change with certain values
  if (style.willChange.includes('transform') ||
      style.willChange.includes('opacity') ||
      style.willChange.includes('filter')) return true;

  // contain: paint/layout/strict/content
  if (style.contain.includes('paint') || style.contain.includes('layout')) return true;

  // clip-path != none
  if (style.clipPath !== 'none') return true;

  // mask != none
  if (style.mask !== 'none') return true;

  // mix-blend-mode != normal
  if (style.mixBlendMode !== 'normal') return true;

  return false;
}
```

**6.2 is_stacked and has_layer heuristics**
```javascript
function isStacked(element) {
  const style = getComputedStyle(element);
  // Element participates in stacking (positioned or creates stacking context)
  return style.position !== 'static' || createsStackingContext(element);
}

function hasLayer(element) {
  const style = getComputedStyle(element);
  // Approximation: elements that would get their own compositor layer
  return (
    createsStackingContext(element) ||
    style.overflow !== 'visible' ||
    style.transform !== 'none' ||
    style.willChange !== 'auto'
  );
}
```

---

### Phase 7: Layout Object Naming

**7.1 Generate Chromium-style layout object names**
```javascript
function getLayoutObjectName(node) {
  if (node.nodeType === Node.TEXT_NODE) {
    return 'LayoutText #text';
  }

  const tag = node.tagName;
  const style = getComputedStyle(node);
  const display = style.display;

  let layoutType;

  switch (display) {
    case 'block':
    case 'flow-root':
      layoutType = 'LayoutBlockFlow';
      break;
    case 'inline':
      layoutType = 'LayoutInline';
      break;
    case 'inline-block':
      layoutType = 'LayoutBlockFlow'; // inline-block creates block formatting context
      break;
    case 'flex':
      layoutType = 'LayoutFlexibleBox';
      break;
    case 'inline-flex':
      layoutType = 'LayoutFlexibleBox';
      break;
    case 'grid':
      layoutType = 'LayoutGrid';
      break;
    case 'inline-grid':
      layoutType = 'LayoutGrid';
      break;
    case 'table':
      layoutType = 'LayoutTable';
      break;
    case 'table-row':
      layoutType = 'LayoutTableRow';
      break;
    case 'table-cell':
      layoutType = 'LayoutTableCell';
      break;
    case 'none':
      layoutType = 'LayoutNone';
      break;
    default:
      layoutType = 'LayoutBlockFlow';
  }

  // Check for children-inline hint
  const hasInlineChildren = Array.from(node.childNodes).some(child => {
    if (child.nodeType === Node.TEXT_NODE) return child.textContent.trim();
    if (child.nodeType === Node.ELEMENT_NODE) {
      const childDisplay = getComputedStyle(child).display;
      return childDisplay === 'inline' || childDisplay === 'inline-block';
    }
    return false;
  });

  let name = layoutType;
  if (hasInlineChildren && layoutType === 'LayoutBlockFlow') {
    name += ' (children-inline)';
  }

  // Add tag
  name += ` ${tag}`;

  // Add id if present
  if (node.id) {
    name += ` id='${node.id}'#${node.id}`;
  }

  return name;
}
```

---

### Phase 8: Main Extraction Function

```javascript
function extractLayoutTree(root = document.documentElement) {
  const nodes = [];
  let currentId = 0;
  const nodeToId = new Map();

  function processNode(node, depth, parentId) {
    const id = currentId++;
    nodeToId.set(node, id);

    const isTextNode = node.nodeType === Node.TEXT_NODE;
    const element = isTextNode ? node.parentElement : node;

    const entry = {
      id,
      name: getLayoutObjectName(node),
      z_index: isTextNode ? 0 : (parseInt(getComputedStyle(node).zIndex) || 0),
      is_stacking_context: isTextNode ? false : createsStackingContext(node),
      is_stacked: isTextNode ? false : isStacked(node),
      has_layer: isTextNode ? false : hasLayer(node),
      computed_style: isTextNode ? getTextNodeStyle(node) : getComputedStyleInfo(node),
      depth,
      children: []
    };

    // Add tag for elements
    if (!isTextNode) {
      entry.tag = node.tagName.toLowerCase();
    }

    // Geometry
    if (isTextNode) {
      const fragments = getTextFragments(node);
      if (fragments.length > 0) {
        entry.text = node.textContent;
        entry.fragments = fragments;
      }
    } else {
      entry.geometry = getGeometry(node);
      entry.padding = getPadding(node);
      entry.margin = getMargin(node);

      // Optional properties
      const bgColor = getBackgroundColor(node);
      if (bgColor) entry.background_color = bgColor;

      const borderRadii = getBorderRadii(node);
      if (borderRadii) entry.border_radii = borderRadii;

      const borderWidths = getBorderWidths(node);
      if (borderWidths) {
        entry.border_widths = borderWidths;
        entry.border_colors = getBorderColors(node);
      }

      const boxShadow = parseBoxShadow(getComputedStyle(node).boxShadow);
      if (boxShadow) entry.box_shadow = boxShadow;
    }

    // is_self_painting (when has_layer is true)
    if (entry.has_layer && !isTextNode) {
      entry.is_self_painting = createsStackingContext(node);
    }

    nodes.push(entry);

    // Process children
    if (!isTextNode) {
      const childIds = [];
      for (const child of node.childNodes) {
        if (child.nodeType === Node.ELEMENT_NODE) {
          const tag = child.tagName.toLowerCase();
          if (!['script', 'style', 'noscript'].includes(tag)) {
            childIds.push(processNode(child, depth + 1, id));
          }
        } else if (child.nodeType === Node.TEXT_NODE && child.textContent.trim()) {
          childIds.push(processNode(child, depth + 1, id));
        }
      }
      entry.children = childIds;
    }

    return id;
  }

  // Start with LayoutView wrapper
  const layoutView = {
    id: currentId++,
    name: 'LayoutView #document',
    z_index: 0,
    is_stacking_context: true,
    is_stacked: true,
    has_layer: true,
    is_self_painting: true,
    computed_style: {
      display: 'block',
      position: 'absolute',
      opacity: 1,
      z_index: 0,
      has_transform: false,
      overflow_x: 'auto',
      overflow_y: 'auto',
      visibility: 'visible',
      has_filter: false,
      has_backdrop_filter: false,
      has_clip_path: false,
      has_mask: false,
      is_isolated: false,
      has_will_change_transform: false,
      has_will_change_opacity: false,
      contain_paint: false,
      contain_layout: false,
      letter_spacing: 0,
      word_spacing: 0,
      font_size: parseFloat(getComputedStyle(document.documentElement).fontSize),
      font_family: getComputedStyle(document.documentElement).fontFamily.split(',')[0].replace(/['"]/g, '').trim(),
      font_weight: 400,
      font_style: 'normal'
    },
    geometry: {
      x: 0,
      y: 0,
      width: document.documentElement.scrollWidth,
      height: document.documentElement.scrollHeight
    },
    padding: { top: 0, right: 0, bottom: 0, left: 0 },
    margin: { top: 0, right: 0, bottom: 0, left: 0 },
    depth: 0,
    children: []
  };

  // Add background color if body has one
  const bodyBg = getBackgroundColor(document.body);
  if (bodyBg) {
    layoutView.background_color = bodyBg;
  }

  nodes.push(layoutView);

  // Process DOM tree
  const rootId = processNode(root, 1, 0);
  layoutView.children = [rootId];

  // Sort by ID for consistent output
  nodes.sort((a, b) => a.id - b.id);

  return { layout_tree: nodes };
}

// Usage:
// const tree = extractLayoutTree();
// console.log(JSON.stringify(tree, null, 2));
```

---

## Known Limitations

| Aspect | Chromium Internal | JS Extraction | Notes |
|--------|------------------|---------------|-------|
| Layout object types | Exact (LayoutBlockFlow, LayoutInline, etc.) | Inferred from `display` | Close but not identical |
| Anonymous boxes | Visible | Hidden | Can't detect internal wrappers |
| `is_self_painting` | Exact from PaintLayer | Heuristic | Based on stacking context |
| `has_layer` | Exact from compositor | Heuristic | Approximation |
| Text fragments character ranges | Exact | Approximated | Would need character-level measurement |
| Glyph/position data | Available | Not accessible | Font rendering internals hidden |
| Subpixel precision | LayoutUnit | DOMRect doubles | May differ slightly |

---

## Testing Strategy

1. **Visual comparison**: Render both JSONs and compare visually
2. **Property-by-property diff**: Compare specific properties for known elements
3. **Boundary cases**: Test with complex layouts (nested flex/grid, transforms, stacking contexts)

---

## Files to Create

1. `js/layout_tree_extractor.js` - Main extraction script
2. `js/utils/color_parser.js` - Color parsing utilities
3. `js/utils/stacking_context.js` - Stacking context detection
4. `js/utils/box_shadow_parser.js` - Box shadow parsing

---

## Next Steps

1. Implement the extraction script
2. Test on the same page that generated `layout_tree_pre_shape.json`
3. Compare outputs and iterate on accuracy
4. Document any systematic differences
