   // ============================================
    // LAYOUT TREE EXTRACTION
    // ============================================

    function parseColor(colorStr) {
        if (!colorStr || colorStr === 'transparent' || colorStr === 'rgba(0, 0, 0, 0)') {
            return null;
        }
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

    function parseBoxShadow(shadowStr) {
        if (!shadowStr || shadowStr === 'none') return null;
        const shadows = [];
        // Computed style format: [inset] rgba(...) offset-x offset-y blur [spread]
        // or: rgba(...) offset-x offset-y blur [spread] [inset]
        const shadowRegex = /(inset\s+)?(rgba?\([^)]+\))\s+(-?[\d.]+)px\s+(-?[\d.]+)px\s+(-?[\d.]+)px(?:\s+(-?[\d.]+)px)?(\s+inset)?/gi;
        let match;
        while ((match = shadowRegex.exec(shadowStr)) !== null) {
            const color = parseColor(match[2]);
            if (color) {
                shadows.push({
                    inset: !!(match[1] || match[7]),
                    offset_x: parseFloat(match[3]),
                    offset_y: parseFloat(match[4]),
                    blur: parseFloat(match[5]),
                    spread: match[6] ? parseFloat(match[6]) : 0,
                    color: color
                });
            }
        }
        return shadows.length > 0 ? shadows : null;
    }

    function createsStackingContext(element) {
        if (element === document.documentElement) return true;
        const style = getComputedStyle(element);
        if (style.position !== 'static' && style.zIndex !== 'auto') return true;
        const parent = element.parentElement;
        if (parent) {
            const parentDisplay = getComputedStyle(parent).display;
            if ((parentDisplay === 'flex' || parentDisplay === 'grid' ||
                 parentDisplay === 'inline-flex' || parentDisplay === 'inline-grid') &&
                style.zIndex !== 'auto') {
                return true;
            }
        }
        if (parseFloat(style.opacity) < 1) return true;
        if (style.transform !== 'none') return true;
        if (style.filter !== 'none') return true;
        if (style.backdropFilter && style.backdropFilter !== 'none') return true;
        if (style.isolation === 'isolate') return true;
        if (style.willChange && (
            style.willChange.includes('transform') ||
            style.willChange.includes('opacity') ||
            style.willChange.includes('filter'))) return true;
        if (style.contain && (style.contain.includes('paint') || style.contain.includes('layout'))) return true;
        if (style.clipPath && style.clipPath !== 'none') return true;
        if (style.mask && style.mask !== 'none') return true;
        if (style.webkitMask && style.webkitMask !== 'none') return true;
        if (style.mixBlendMode && style.mixBlendMode !== 'normal') return true;
        return false;
    }

    function isStacked(element) {
        const style = getComputedStyle(element);
        return style.position !== 'static' || createsStackingContext(element);
    }

    function hasLayer(element) {
        const style = getComputedStyle(element);
        return (
            createsStackingContext(element) ||
            (style.overflow !== 'visible' && style.overflow !== '') ||
            style.transform !== 'none' ||
            (style.willChange && style.willChange !== 'auto')
        );
    }

    // Check if a pseudo-element exists and has content
    function hasPseudoElement(element, pseudo) {
        const style = getComputedStyle(element, pseudo);
        const content = style.content;
        // content is 'none' or 'normal' if pseudo-element doesn't exist
        // content: "" (empty string) IS valid and creates a pseudo-element
        return content && content !== 'none' && content !== 'normal';
    }

    // Get computed style info for a pseudo-element
    function getPseudoComputedStyleInfo(element, pseudo) {
        const style = getComputedStyle(element, pseudo);
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
            has_backdrop_filter: style.backdropFilter ? style.backdropFilter !== 'none' : false,
            has_clip_path: style.clipPath ? style.clipPath !== 'none' : false,
            has_mask: false,
            is_isolated: style.isolation === 'isolate',
            has_will_change_transform: false,
            has_will_change_opacity: false,
            contain_paint: false,
            contain_layout: false,
            letter_spacing: parseFloat(style.letterSpacing) || 0,
            word_spacing: parseFloat(style.wordSpacing) || 0,
            font_size: parseFloat(style.fontSize),
            font_family: style.fontFamily.split(',')[0].replace(/['"]/g, '').trim(),
            font_weight: parseInt(style.fontWeight),
            font_style: style.fontStyle
        };
    }

    // Estimate pseudo-element geometry based on parent and styles
    function getPseudoGeometry(element, pseudo) {
        const parentRect = element.getBoundingClientRect();
        const style = getComputedStyle(element, pseudo);

        // Get dimensions
        let width = parseFloat(style.width) || 0;
        let height = parseFloat(style.height) || 0;

        // For positioned pseudo-elements, calculate position
        const position = style.position;
        let x = parentRect.x;
        let y = parentRect.y;

        if (position === 'absolute') {
            const top = parseFloat(style.top);
            const left = parseFloat(style.left);
            const right = parseFloat(style.right);
            const bottom = parseFloat(style.bottom);

            if (!isNaN(top)) y = parentRect.y + top;
            else if (!isNaN(bottom)) y = parentRect.y + parentRect.height - bottom - height;

            if (!isNaN(left)) x = parentRect.x + left;
            else if (!isNaN(right)) x = parentRect.x + parentRect.width - right - width;
        }

        return {
            x: x + window.scrollX,
            y: y + window.scrollY,
            width: width,
            height: height
        };
    }

    // Create a pseudo-element node
    function createPseudoElementNode(id, element, pseudo, depth) {
        const style = getComputedStyle(element, pseudo);
        const position = style.position;
        const isPositioned = position !== 'static';

        let name = 'LayoutBlockFlow';
        if (isPositioned) name += ' (positioned, children-inline)';
        else name += ' (children-inline)';
        name += ` ${pseudo}`;

        const entry = {
            id,
            name,
            z_index: style.zIndex === 'auto' ? 0 : parseInt(style.zIndex),
            is_stacking_context: isPositioned && style.zIndex !== 'auto',
            is_stacked: isPositioned,
            has_layer: isPositioned,
            computed_style: getPseudoComputedStyleInfo(element, pseudo),
            geometry: getPseudoGeometry(element, pseudo),
            padding: {
                top: parseFloat(style.paddingTop) || 0,
                right: parseFloat(style.paddingRight) || 0,
                bottom: parseFloat(style.paddingBottom) || 0,
                left: parseFloat(style.paddingLeft) || 0
            },
            margin: {
                top: parseFloat(style.marginTop) || 0,
                right: parseFloat(style.marginRight) || 0,
                bottom: parseFloat(style.marginBottom) || 0,
                left: parseFloat(style.marginLeft) || 0
            },
            depth,
            children: []
        };

        // Add background color if present
        const bgColor = parseColor(style.backgroundColor);
        if (bgColor && bgColor.a > 0) entry.background_color = bgColor;

        // Add border radius if present
        const tl = parseFloat(style.borderTopLeftRadius) || 0;
        const tr = parseFloat(style.borderTopRightRadius) || 0;
        const br = parseFloat(style.borderBottomRightRadius) || 0;
        const bl = parseFloat(style.borderBottomLeftRadius) || 0;
        if (tl || tr || br || bl) {
            entry.border_radii = [tl, tl, tr, tr, br, br, bl, bl];
        }

        // Add box shadow if present
        const boxShadow = parseBoxShadow(style.boxShadow);
        if (boxShadow) entry.box_shadow = boxShadow;

        return entry;
    }

    // Detect form element types that have shadow DOM
    function getFormElementInfo(element) {
        const tag = element.tagName;
        if (tag === 'INPUT') {
            const type = element.type || 'text';
            if (['text', 'email', 'password', 'search', 'tel', 'url'].includes(type)) {
                return { type: 'text-control-single', inputType: type };
            } else if (type === 'range') {
                return { type: 'slider', inputType: type };
            }
        } else if (tag === 'TEXTAREA') {
            return { type: 'text-control-multi' };
        } else if (tag === 'SELECT') {
            return { type: 'select' };
        }
        return null;
    }

    // Create shadow DOM children for text input controls
    function createTextControlShadowChildren(element, parentId, depth, nodes, currentIdRef) {
        const rect = element.getBoundingClientRect();
        const style = getComputedStyle(element);
        const childIds = [];

        // Create LayoutTextControlInnerEditor
        const editorId = currentIdRef.id++;
        const editorEntry = {
            id: editorId,
            name: 'LayoutTextControlInnerEditor DIV',
            z_index: 0,
            is_stacking_context: false,
            is_stacked: false,
            has_layer: false,
            computed_style: {
                display: 'block',
                position: 'static',
                opacity: 1,
                z_index: 0,
                has_transform: false,
                overflow_x: 'visible',
                overflow_y: 'visible',
                visibility: style.visibility,
                has_filter: false,
                has_backdrop_filter: false,
                has_clip_path: false,
                has_mask: false,
                is_isolated: false,
                has_will_change_transform: false,
                has_will_change_opacity: false,
                contain_paint: false,
                contain_layout: false,
                letter_spacing: parseFloat(style.letterSpacing) || 0,
                word_spacing: parseFloat(style.wordSpacing) || 0,
                font_size: parseFloat(style.fontSize),
                font_family: style.fontFamily.split(',')[0].replace(/['"]/g, '').trim(),
                font_weight: parseInt(style.fontWeight),
                font_style: style.fontStyle
            },
            geometry: {
                x: rect.x + window.scrollX + parseFloat(style.paddingLeft),
                y: rect.y + window.scrollY + parseFloat(style.paddingTop),
                width: rect.width - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight),
                height: rect.height - parseFloat(style.paddingTop) - parseFloat(style.paddingBottom)
            },
            padding: { top: 0, right: 0, bottom: 0, left: 0 },
            margin: { top: 0, right: 0, bottom: 0, left: 0 },
            depth: depth,
            children: []
        };
        nodes.push(editorEntry);
        childIds.push(editorId);

        // Create LayoutText for placeholder or value
        const text = element.value || element.placeholder || '';
        if (text) {
            const textId = currentIdRef.id++;
            const textEntry = {
                id: textId,
                name: 'LayoutText #text',
                z_index: 0,
                is_stacking_context: false,
                is_stacked: false,
                has_layer: false,
                computed_style: editorEntry.computed_style,
                text: text,
                fragments: [{
                    x: editorEntry.geometry.x,
                    y: editorEntry.geometry.y,
                    width: editorEntry.geometry.width,
                    height: parseFloat(style.lineHeight) || parseFloat(style.fontSize) * 1.2,
                    start: 0,
                    end: text.length,
                    runs: [{ glyphs: null, positions: null }]
                }],
                depth: depth + 1,
                children: []
            };
            nodes.push(textEntry);
            editorEntry.children.push(textId);
        }

        return childIds;
    }

    function getLayoutObjectName(node) {
        if (node.nodeType === Node.TEXT_NODE) {
            return 'LayoutText #text';
        }
        const tag = node.tagName;

        // Special case for BR element
        if (tag === 'BR') {
            return 'LayoutBR BR';
        }

        // Special cases for form elements with shadow DOM
        const formInfo = getFormElementInfo(node);
        if (formInfo) {
            if (formInfo.type === 'text-control-single') {
                return 'LayoutTextControlSingleLine (inline) INPUT';
            } else if (formInfo.type === 'text-control-multi') {
                return 'LayoutTextControlMultiLine (inline) TEXTAREA';
            } else if (formInfo.type === 'select') {
                return 'LayoutFlexibleBox (inline) SELECT';
            } else if (formInfo.type === 'slider') {
                return 'LayoutFlexibleBox (inline) INPUT';
            }
        }

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
                layoutType = 'LayoutBlockFlow';
                break;
            case 'flex':
            case 'inline-flex':
                layoutType = 'LayoutFlexibleBox';
                break;
            case 'grid':
            case 'inline-grid':
                layoutType = 'LayoutGrid';
                break;
            case 'table':
                layoutType = 'LayoutTable';
                break;
            case 'table-row-group':
                layoutType = 'LayoutTableSection';
                break;
            case 'table-header-group':
                layoutType = 'LayoutTableSection';
                break;
            case 'table-row':
                layoutType = 'LayoutTableRow';
                break;
            case 'table-cell':
                layoutType = 'LayoutTableCell';
                break;
            default:
                layoutType = 'LayoutBlockFlow';
        }
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
        name += ` ${tag}`;
        // Add class name if present
        if (node.className && typeof node.className === 'string' && node.className.trim()) {
            const firstClass = node.className.trim().split(/\s+/)[0];
            name += ` class='${firstClass}'.${firstClass}`;
        }
        // Add id if present
        if (node.id) {
            name += ` id='${node.id}'#${node.id}`;
        }
        return name;
    }

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
            has_backdrop_filter: style.backdropFilter ? style.backdropFilter !== 'none' : false,
            has_clip_path: style.clipPath ? style.clipPath !== 'none' : false,
            has_mask: (style.mask && style.mask !== 'none') || (style.maskImage && style.maskImage !== 'none'),
            is_isolated: style.isolation === 'isolate',
            has_will_change_transform: style.willChange ? style.willChange.includes('transform') : false,
            has_will_change_opacity: style.willChange ? style.willChange.includes('opacity') : false,
            contain_paint: style.contain ? style.contain.includes('paint') : false,
            contain_layout: style.contain ? style.contain.includes('layout') : false,
            letter_spacing: parseFloat(style.letterSpacing) || 0,
            word_spacing: parseFloat(style.wordSpacing) || 0,
            font_size: parseFloat(style.fontSize),
            font_family: style.fontFamily.split(',')[0].replace(/['"]/g, '').trim(),
            font_weight: parseInt(style.fontWeight),
            font_style: style.fontStyle
        };
    }

    function getGeometry(element) {
        const rect = element.getBoundingClientRect();
        return {
            x: rect.x + window.scrollX,
            y: rect.y + window.scrollY,
            width: rect.width,
            height: rect.height
        };
    }

    function getTextFragments(textNode) {
        const range = document.createRange();
        range.selectNodeContents(textNode);
        const rects = range.getClientRects();
        return Array.from(rects).map((rect) => ({
            x: rect.x + window.scrollX,
            y: rect.y + window.scrollY,
            width: rect.width,
            height: rect.height,
            start: 0,
            end: textNode.textContent.length,
            runs: [{ glyphs: null, positions: null }]
        }));
    }

    function getPadding(element) {
        const style = getComputedStyle(element);
        return {
            top: parseFloat(style.paddingTop) || 0,
            right: parseFloat(style.paddingRight) || 0,
            bottom: parseFloat(style.paddingBottom) || 0,
            left: parseFloat(style.paddingLeft) || 0
        };
    }

    function getMargin(element) {
        const style = getComputedStyle(element);
        return {
            top: parseFloat(style.marginTop) || 0,
            right: parseFloat(style.marginRight) || 0,
            bottom: parseFloat(style.marginBottom) || 0,
            left: parseFloat(style.marginLeft) || 0
        };
    }

    function getBorderWidths(element) {
        const style = getComputedStyle(element);
        const widths = {
            top: parseFloat(style.borderTopWidth) || 0,
            right: parseFloat(style.borderRightWidth) || 0,
            bottom: parseFloat(style.borderBottomWidth) || 0,
            left: parseFloat(style.borderLeftWidth) || 0
        };
        if (widths.top || widths.right || widths.bottom || widths.left) {
            return widths;
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

    function getBorderRadii(element) {
        const style = getComputedStyle(element);
        const tl = parseFloat(style.borderTopLeftRadius) || 0;
        const tr = parseFloat(style.borderTopRightRadius) || 0;
        const br = parseFloat(style.borderBottomRightRadius) || 0;
        const bl = parseFloat(style.borderBottomLeftRadius) || 0;
        const radii = [tl, tl, tr, tr, br, br, bl, bl];
        if (radii.some(r => r > 0)) {
            return radii;
        }
        return null;
    }

    function getBackgroundColor(element) {
        const style = getComputedStyle(element);
        const color = parseColor(style.backgroundColor);
        if (color && color.a > 0) {
            return color;
        }
        return null;
    }

    // Check if a display value creates a block-level box
    function isBlockLevel(display) {
        return ['block', 'flex', 'grid', 'table', 'flow-root',
                'table-row-group', 'table-header-group', 'table-footer-group',
                'table-row', 'table-cell', 'table-caption', 'list-item'].includes(display);
    }

    // Create an anonymous block wrapper
    function createAnonymousBlock(id, parentElement, depth, inlineChildren) {
        const parentStyle = getComputedStyle(parentElement);
        const parentRect = parentElement.getBoundingClientRect();

        // Calculate geometry from inline children
        let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
        for (const child of inlineChildren) {
            if (child.isText) {
                const range = document.createRange();
                range.selectNodeContents(child.node);
                const rects = range.getClientRects();
                for (const rect of rects) {
                    minX = Math.min(minX, rect.x);
                    minY = Math.min(minY, rect.y);
                    maxX = Math.max(maxX, rect.x + rect.width);
                    maxY = Math.max(maxY, rect.y + rect.height);
                }
            } else {
                const rect = child.node.getBoundingClientRect();
                minX = Math.min(minX, rect.x);
                minY = Math.min(minY, rect.y);
                maxX = Math.max(maxX, rect.x + rect.width);
                maxY = Math.max(maxY, rect.y + rect.height);
            }
        }

        // If no geometry found (e.g., all whitespace), use parent geometry
        if (minX === Infinity) {
            minX = parentRect.x;
            minY = parentRect.y;
            maxX = parentRect.x;
            maxY = parentRect.y;
        }

        return {
            id,
            name: 'LayoutBlockFlow (anonymous, children-inline)',
            z_index: 0,
            is_stacking_context: false,
            is_stacked: false,
            has_layer: false,
            computed_style: {
                display: 'block',
                position: 'static',
                opacity: parseFloat(parentStyle.opacity),
                z_index: 0,
                has_transform: false,
                overflow_x: 'visible',
                overflow_y: 'visible',
                visibility: parentStyle.visibility,
                has_filter: false,
                has_backdrop_filter: false,
                has_clip_path: false,
                has_mask: false,
                is_isolated: false,
                has_will_change_transform: false,
                has_will_change_opacity: false,
                contain_paint: false,
                contain_layout: false,
                letter_spacing: parseFloat(parentStyle.letterSpacing) || 0,
                word_spacing: parseFloat(parentStyle.wordSpacing) || 0,
                font_size: parseFloat(parentStyle.fontSize),
                font_family: parentStyle.fontFamily.split(',')[0].replace(/['"]/g, '').trim(),
                font_weight: parseInt(parentStyle.fontWeight),
                font_style: parentStyle.fontStyle
            },
            geometry: {
                x: minX + window.scrollX,
                y: minY + window.scrollY,
                width: maxX - minX,
                height: maxY - minY
            },
            padding: { top: 0, right: 0, bottom: 0, left: 0 },
            margin: { top: 0, right: 0, bottom: 0, left: 0 },
            depth,
            children: []
        };
    }

    function extractLayoutTree(root = document.documentElement) {
        const nodes = [];
        let currentId = 0;

        function processNode(node, depth) {
            const id = currentId++;
            const isTextNode = node.nodeType === Node.TEXT_NODE;
            const element = isTextNode ? node.parentElement : node;

            const entry = {
                id,
                name: getLayoutObjectName(node),
                z_index: isTextNode ? 0 : (parseInt(getComputedStyle(node).zIndex) || 0),
                is_stacking_context: isTextNode ? false : createsStackingContext(node),
                is_stacked: isTextNode ? false : isStacked(node),
                has_layer: isTextNode ? false : hasLayer(node),
                computed_style: isTextNode ? getComputedStyleInfo(element) : getComputedStyleInfo(node),
                depth,
                children: []
            };

            if (!isTextNode) {
                entry.tag = node.tagName.toLowerCase();
            }

            if (isTextNode) {
                entry.text = node.textContent;
                const fragments = getTextFragments(node);
                if (fragments.length > 0) {
                    entry.fragments = fragments;
                } else {
                    // Whitespace text node - no visible geometry but still exists
                    entry.fragments = [];
                }
            } else {
                entry.geometry = getGeometry(node);
                entry.padding = getPadding(node);
                entry.margin = getMargin(node);

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

            if (entry.has_layer && !isTextNode) {
                entry.is_self_painting = createsStackingContext(node);
            }

            nodes.push(entry);

            if (!isTextNode) {
                const childIds = [];
                const style = getComputedStyle(node);
                const display = style.display;
                const isFlexOrGrid = ['flex', 'inline-flex', 'grid', 'inline-grid'].includes(display);
                const isBlock = ['block', 'flow-root'].includes(display);

                // Check for form elements with shadow DOM
                const formInfo = getFormElementInfo(node);
                if (formInfo && (formInfo.type === 'text-control-single' || formInfo.type === 'text-control-multi')) {
                    // Create shadow DOM children for text controls
                    const currentIdRef = { id: currentId };
                    const shadowChildIds = createTextControlShadowChildren(node, id, depth + 1, nodes, currentIdRef);
                    currentId = currentIdRef.id;
                    childIds.push(...shadowChildIds);
                    entry.children = childIds;
                    return id;
                }

                // Check for ::before pseudo-element
                if (hasPseudoElement(node, '::before')) {
                    const beforeId = currentId++;
                    const beforeEntry = createPseudoElementNode(beforeId, node, '::before', depth + 1);
                    nodes.push(beforeEntry);
                    childIds.push(beforeId);

                    // Create text fragment child for the pseudo-element content
                    const textFragId = currentId++;
                    const textFragEntry = {
                        id: textFragId,
                        name: 'LayoutTextFragment (anonymous)',
                        z_index: 0,
                        is_stacking_context: false,
                        is_stacked: false,
                        has_layer: false,
                        computed_style: beforeEntry.computed_style,
                        depth: depth + 2,
                        children: []
                    };
                    nodes.push(textFragEntry);
                    beforeEntry.children.push(textFragId);
                }

                // Collect valid children
                // Whitespace text nodes are only kept when between inline elements (not at boundaries)
                const rawChildren = [];
                for (const child of node.childNodes) {
                    if (child.nodeType === Node.ELEMENT_NODE) {
                        const tag = child.tagName.toLowerCase();
                        if (!['script', 'style', 'noscript'].includes(tag)) {
                            const childStyle = getComputedStyle(child);
                            if (childStyle.display !== 'none') {
                                rawChildren.push({ node: child, isText: false, isBlock: isBlockLevel(childStyle.display), isWhitespace: false });
                            }
                        }
                    } else if (child.nodeType === Node.TEXT_NODE) {
                        if (child.textContent.trim()) {
                            rawChildren.push({ node: child, isText: true, isBlock: false, isWhitespace: false });
                        } else if (child.textContent.length > 0) {
                            rawChildren.push({ node: child, isText: true, isBlock: false, isWhitespace: true });
                        }
                    }
                }

                // Filter: keep whitespace only when it's trailing after an inline element
                // (Chromium keeps trailing whitespace after inline elements in anonymous blocks)
                const validChildren = [];
                for (let i = 0; i < rawChildren.length; i++) {
                    const child = rawChildren[i];
                    if (!child.isWhitespace) {
                        validChildren.push(child);
                    } else {
                        // Check adjacent elements
                        const prevNonWS = rawChildren.slice(0, i).reverse().find(c => !c.isWhitespace);
                        const nextNonWS = rawChildren.slice(i + 1).find(c => !c.isWhitespace);
                        // Keep whitespace if:
                        // 1. It's after an inline element AND (followed by a block OR is last in run)
                        const afterInline = prevNonWS && !prevNonWS.isBlock;
                        const beforeBlockOrEnd = !nextNonWS || nextNonWS.isBlock;
                        if (afterInline && beforeBlockOrEnd) {
                            validChildren.push(child);
                        }
                    }
                }

                // Check if we have mixed block/inline content
                const hasBlockChild = validChildren.some(c => c.isBlock);
                const hasInlineContent = validChildren.some(c => !c.isBlock);
                const needsAnonymousBlocks = (isFlexOrGrid && hasInlineContent) ||
                                              (isBlock && hasBlockChild && hasInlineContent);

                if (needsAnonymousBlocks) {
                    // Group consecutive inline content and wrap in anonymous blocks
                    let inlineRun = [];

                    const flushInlineRun = () => {
                        if (inlineRun.length > 0) {
                            // Create anonymous block for this inline run
                            const anonId = currentId++;
                            const anonEntry = createAnonymousBlock(anonId, node, depth + 1, inlineRun);
                            nodes.push(anonEntry);

                            // Process children of anonymous block
                            const anonChildIds = [];
                            for (const inlineChild of inlineRun) {
                                anonChildIds.push(processNode(inlineChild.node, depth + 2));
                            }
                            anonEntry.children = anonChildIds;

                            childIds.push(anonId);
                            inlineRun = [];
                        }
                    };

                    for (const child of validChildren) {
                        if (child.isBlock) {
                            flushInlineRun();
                            childIds.push(processNode(child.node, depth + 1));
                        } else {
                            inlineRun.push(child);
                        }
                    }
                    flushInlineRun();
                } else {
                    // No anonymous blocks needed
                    for (const child of validChildren) {
                        childIds.push(processNode(child.node, depth + 1));
                    }
                }

                // Check for ::after pseudo-element
                if (hasPseudoElement(node, '::after')) {
                    const afterId = currentId++;
                    const afterEntry = createPseudoElementNode(afterId, node, '::after', depth + 1);
                    nodes.push(afterEntry);
                    childIds.push(afterId);

                    // Create text fragment child for the pseudo-element content
                    const textFragId = currentId++;
                    const textFragEntry = {
                        id: textFragId,
                        name: 'LayoutTextFragment (anonymous)',
                        z_index: 0,
                        is_stacking_context: false,
                        is_stacked: false,
                        has_layer: false,
                        computed_style: afterEntry.computed_style,
                        depth: depth + 2,
                        children: []
                    };
                    nodes.push(textFragEntry);
                    afterEntry.children.push(textFragId);
                }

                entry.children = childIds;
            }

            return id;
        }

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

        const bodyBg = getBackgroundColor(document.body);
        if (bodyBg) {
            layoutView.background_color = bodyBg;
        }

        nodes.push(layoutView);

        const rootId = processNode(root, 1);
        layoutView.children = [rootId];

        nodes.sort((a, b) => a.id - b.id);

        return { layout_tree: nodes };
    }

    // ============================================
    // WEBSOCKET CLIENT
    // ============================================

    window.addEventListener('load', function() {
        setTimeout(function() {
            const ws = new WebSocket('ws://localhost:8765');

            ws.onopen = function() {
                console.log('[WS] Connected to server');
                const tree = extractLayoutTree();
                ws.send(JSON.stringify({
                    type: 'layout_tree',
                    data: tree
                }));
                console.log('[WS] Sent layout tree with', tree.layout_tree.length, 'nodes');
            };

            ws.onmessage = function(event) {
                console.log('[WS] Received:', JSON.parse(event.data));
            };

            ws.onerror = function(err) {
                console.log('[WS] Error:', err);
            };
        }, 1000);
    });