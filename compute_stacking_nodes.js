#!/usr/bin/env node
/**
 * Computes PaintLayerStackingNode structure from layer_tree.json
 *
 * This replicates Chromium's stacking node logic:
 * - A stacking node is created when:
 *   1. The element is a stacking context (is_stacking_context: true)
 *   2. AND it has stacked descendants within its stacking context
 *
 * Each stacking node maintains two z-order lists:
 * - neg_z_order_list: children with z-index < 0
 * - pos_z_order_list: children with z-index >= 0
 *
 * NOTE: This now operates on the PaintLayer tree (layer_tree.json),
 * which is a sparse subset of the LayoutObject tree where only nodes
 * with has_layer: true exist. This matches Chromium's actual traversal.
 *
 * Now uses unique IDs from Chromium's logging to correctly handle duplicate names.
 */

const fs = require('fs');
const path = require('path');

// Load the layer tree (computed from layout_tree.json by compute_layer_tree.js)
const layerTreePath = path.join(__dirname, 'computed_layer_tree.json');
const data = JSON.parse(fs.readFileSync(layerTreePath, 'utf8'));
const layerTree = data.layer_tree;

// Build a map for quick node lookup by ID
const nodeById = new Map();
layerTree.forEach(node => {
  nodeById.set(node.id, node);
});

/**
 * Find the containing stacking context for a layer node
 * (walks up the PaintLayer tree until it finds an ancestor with is_stacking_context: true)
 */
function findContainingStackingContext(node) {
  if (node.parent_id === null || node.parent_id === undefined) return null;
  let current = nodeById.get(node.parent_id);
  while (current) {
    if (current.is_stacking_context) {
      return current;
    }
    if (current.parent_id === null || current.parent_id === undefined) break;
    current = nodeById.get(current.parent_id);
  }
  return null;
}

/**
 * Collect stacked descendants for a stacking context
 * Stops recursion at nested stacking contexts (they handle their own descendants)
 *
 * This traverses the PaintLayer tree (not LayoutObject tree), matching
 * Chromium's CollectLayers() in paint_layer_stacking_node.cc:315-338
 */
function collectStackedDescendants(stackingContext) {
  const stacked = [];

  function walk(node) {
    (node.children || []).forEach(childId => {
      const child = nodeById.get(childId);
      if (!child) return;

      // If this child is stacked, add it to the list
      // (matches: if (object.IsStacked()) in CollectLayers)
      if (child.is_stacked) {
        stacked.push(child);
      }

      // If this child is a stacking context, don't recurse into it
      // (it will handle its own stacked descendants)
      // (matches: if (object.IsStackingContext()) return; in CollectLayers)
      if (!child.is_stacking_context) {
        walk(child);
      }
    });
  }

  walk(stackingContext);
  return stacked;
}

/**
 * Check if a stacking context has stacked descendants
 * Traverses the PaintLayer tree, stopping at nested stacking contexts
 */
function hasStackedDescendantInCurrentStackingContext(node) {
  if (!node.is_stacking_context) return false;

  function hasStacked(current) {
    for (const childId of (current.children || [])) {
      const child = nodeById.get(childId);
      if (!child) continue;

      if (child.is_stacked) return true;

      // Don't recurse into nested stacking contexts
      if (!child.is_stacking_context && hasStacked(child)) {
        return true;
      }
    }
    return false;
  }

  return hasStacked(node);
}

/**
 * Compute stacking nodes for all stacking contexts in the PaintLayer tree
 *
 * Matches Chromium's UpdateStackingNode() in paint_layer.cc:858-877:
 *   bool needs_stacking_node =
 *       has_stacked_descendant_in_current_stacking_context_ &&
 *       GetLayoutObject().IsStackingContext();
 */
function computeStackingNodes() {
  const stackingNodes = [];

  layerTree.forEach(node => {
    if (!node.is_stacking_context) return;

    const hasStackedDescendant = hasStackedDescendantInCurrentStackingContext(node);

    // Only create stacking node if stacking context has stacked descendants
    const needsStackingNode = hasStackedDescendant;

    // Only output stacking contexts that have stacking nodes (matching stacking_nodes.json format)
    if (needsStackingNode) {
      const stackedDescendants = collectStackedDescendants(node);

      // Split into negative and positive z-index lists
      // Use stable sort: sort by z_index, preserve tree order for equal z_index
      // (matches RebuildZOrderLists stable_sort by EffectiveZIndex)
      const negZOrderList = stackedDescendants
        .map((d, i) => ({...d, _order: i}))  // Track insertion order for stable sort
        .filter(d => d.z_index < 0)
        .sort((a, b) => a.z_index - b.z_index || a._order - b._order);

      const posZOrderList = stackedDescendants
        .map((d, i) => ({...d, _order: i}))  // Track insertion order for stable sort
        .filter(d => d.z_index >= 0)
        .sort((a, b) => a.z_index - b.z_index || a._order - b._order);

      // Output format matches stacking_nodes.json exactly:
      // - layer_id, layer, has_stacking_node
      // - z-order list items: id, name, z_index (no is_stacking_context)
      stackingNodes.push({
        layer_id: node.id,
        layer: node.name,
        has_stacking_node: true,
        neg_z_order_list: negZOrderList.map(n => ({
          id: n.id,
          name: n.name,
          z_index: n.z_index
        })),
        pos_z_order_list: posZOrderList.map(n => ({
          id: n.id,
          name: n.name,
          z_index: n.z_index
        }))
      });
    }
    // Skip stacking contexts without stacking nodes (not in reference output)
  });

  return stackingNodes;
}

/**
 * Generate paint order traversal following CSS 2.1 Appendix E
 * Now operates on the PaintLayer tree using IDs
 */
function generatePaintOrder(stackingContext, indent = 0) {
  const output = [];
  const prefix = '  '.repeat(indent);
  const node = typeof stackingContext === 'number'
    ? nodeById.get(stackingContext)
    : stackingContext;

  if (!node) return output;

  output.push(`${prefix}[PAINT] [${node.id}] ${node.name} (z-index: ${node.z_index})`);

  if (node.is_stacking_context) {
    const stackedDescendants = collectStackedDescendants(node);

    // 1. Negative z-index children
    const negList = stackedDescendants
      .filter(d => d.z_index < 0)
      .sort((a, b) => a.z_index - b.z_index);

    if (negList.length > 0) {
      output.push(`${prefix}  [NEG Z-INDEX]`);
      negList.forEach(child => {
        if (child.is_stacking_context) {
          output.push(...generatePaintOrder(child, indent + 2));
        } else {
          output.push(`${prefix}    [${child.id}] ${child.name} (z-index: ${child.z_index})`);
        }
      });
    }

    // 2. Normal flow content (non-stacked descendants painted in tree order)
    // (simplified - just noting where they would go)
    output.push(`${prefix}  [NORMAL FLOW CONTENT]`);

    // 3. Positive z-index children (including z-index: 0)
    const posList = stackedDescendants
      .filter(d => d.z_index >= 0)
      .sort((a, b) => a.z_index - b.z_index);

    if (posList.length > 0) {
      output.push(`${prefix}  [POS Z-INDEX (incl. 0)]`);
      posList.forEach(child => {
        if (child.is_stacking_context) {
          output.push(...generatePaintOrder(child, indent + 2));
        } else {
          output.push(`${prefix}    [${child.id}] ${child.name} (z-index: ${child.z_index})`);
        }
      });
    }
  }

  return output;
}

// Main execution
const stackingNodes = computeStackingNodes();

// Output results
console.log('='.repeat(80));
console.log('STACKING NODES COMPUTED FROM PAINT LAYER TREE');
console.log('='.repeat(80));
console.log();

// Summary - stackingNodes now only contains those with stacking nodes
console.log(`Stacking contexts with stacking nodes: ${stackingNodes.length}`);
console.log();

// Detailed output for stacking contexts WITH stacking nodes
console.log('='.repeat(80));
console.log('STACKING CONTEXTS WITH STACKING NODES');
console.log('='.repeat(80));

stackingNodes.forEach(sn => {
  console.log();
  console.log(`Stacking Context: [${sn.layer_id}] ${sn.layer}`);
  console.log(`  Negative z-order list (${sn.neg_z_order_list.length}):`);
  sn.neg_z_order_list.forEach(item => {
    console.log(`    - [${item.id}] ${item.name} (z-index: ${item.z_index})`);
  });
  console.log(`  Positive z-order list (${sn.pos_z_order_list.length}):`);
  sn.pos_z_order_list.forEach(item => {
    console.log(`    - [${item.id}] ${item.name} (z-index: ${item.z_index})`);
  });
});

// Paint order from root
console.log();
console.log('='.repeat(80));
console.log('PAINT ORDER (from root stacking context)');
console.log('='.repeat(80));
console.log();

const root = layerTree.find(n => n.layer_depth === 0);
if (root) {
  const paintOrder = generatePaintOrder(root);
  paintOrder.forEach(line => console.log(line));
}

// Write JSON output matching stacking_nodes.json format exactly
// - No summary wrapper
// - Only stacking contexts with has_stacking_node: true
const outputPath = path.join(__dirname, 'computed_stacking_nodes.json');
fs.writeFileSync(outputPath, JSON.stringify({
  stacking_nodes: stackingNodes
}, null, 3));

console.log();
console.log('='.repeat(80));
console.log(`Output written to: ${outputPath}`);
console.log('='.repeat(80));
