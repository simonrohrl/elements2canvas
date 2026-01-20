#!/usr/bin/env node
/**
 * Computes the PaintLayer tree from the LayoutObject tree.
 *
 * This replicates Chromium's logic for determining which LayoutObjects get a PaintLayer:
 * - A PaintLayer is created when LayerTypeRequired() != kNoPaintLayer
 * - The PaintLayer tree is a sparse subset of the LayoutObject tree
 * - Parent-child relationships in PaintLayer tree skip non-layer LayoutObjects
 *
 * Now uses unique IDs from Chromium's logging to correctly handle duplicate names.
 *
 * See: layout_box.cc:498-513 (LayerTypeRequired)
 *      paint_layer.cc:829-852 (InsertOnlyThisLayerAfterStyleChange)
 */

const fs = require('fs');
const path = require('path');

// Load the layout tree
const layoutTreePath = path.join(__dirname, 'layout_tree.json');
const data = JSON.parse(fs.readFileSync(layoutTreePath, 'utf8'));
const layoutTree = data.layout_tree;

// Build a map for quick node lookup by ID
const nodeById = new Map();
layoutTree.forEach(node => {
  nodeById.set(node.id, node);
});

// Build parent references in the LayoutObject tree using IDs
layoutTree.forEach(node => {
  (node.children || []).forEach(childId => {
    const child = nodeById.get(childId);
    if (child) {
      child.parent_id = node.id;
    }
  });
});

/**
 * Find the enclosing layer for a LayoutObject
 * (walks up the LayoutObject tree until finding an ancestor with has_layer: true)
 */
function findEnclosingLayerId(node) {
  let currentId = node.parent_id;
  while (currentId !== undefined) {
    const current = nodeById.get(currentId);
    if (!current) break;
    if (current.has_layer) {
      return current.id;
    }
    currentId = current.parent_id;
  }
  return null;
}

/**
 * Build the PaintLayer tree from the LayoutObject tree
 */
function buildLayerTree() {
  const layerNodes = [];
  const layerById = new Map();

  // First pass: Create layer nodes for all LayoutObjects with has_layer: true
  layoutTree.forEach(node => {
    if (node.has_layer) {
      const layerNode = {
        id: node.id,  // Use same ID as the LayoutObject
        name: node.name,
        z_index: node.z_index,
        is_stacking_context: node.is_stacking_context,
        is_stacked: node.is_stacked,
        tag: node.tag,
        layout_depth: node.depth,
        children: [],
        // We'll compute layer_depth after building the tree
        layer_depth: 0
      };
      layerNodes.push(layerNode);
      layerById.set(node.id, layerNode);
    }
  });

  // Second pass: Build parent-child relationships in the layer tree
  layoutTree.forEach(node => {
    if (node.has_layer) {
      const layerNode = layerById.get(node.id);
      const enclosingLayerId = findEnclosingLayerId(node);

      if (enclosingLayerId !== null) {
        const parentLayerNode = layerById.get(enclosingLayerId);
        if (parentLayerNode) {
          parentLayerNode.children.push(node.id);
          layerNode.parent_id = enclosingLayerId;
        }
      }
    }
  });

  // Third pass: Compute layer depths
  function computeLayerDepth(layerId, depth) {
    const layerNode = layerById.get(layerId);
    if (!layerNode) return;

    layerNode.layer_depth = depth;
    layerNode.children.forEach(childId => {
      computeLayerDepth(childId, depth + 1);
    });
  }

  // Find root layers (no parent)
  const rootLayers = layerNodes.filter(n => n.parent_id === undefined);
  rootLayers.forEach(root => computeLayerDepth(root.id, 0));

  return { layerNodes, layerById };
}

// Build the layer tree
const { layerNodes, layerById } = buildLayerTree();

/**
 * Re-number layer IDs sequentially in depth-first traversal order
 * to match Chromium's PaintLayer ID assignment
 */
function renumberLayerIds(layerNodes, layerById) {
  // Find root layers
  const rootLayers = layerNodes.filter(n => n.parent_id === undefined);

  // Create mapping from old ID to new ID
  const oldToNewId = new Map();
  let newIdCounter = 0;

  // Depth-first traversal to assign new IDs
  function assignNewIds(layerId) {
    const layer = layerById.get(layerId);
    if (!layer) return;

    oldToNewId.set(layerId, newIdCounter);
    newIdCounter++;

    layer.children.forEach(childId => {
      assignNewIds(childId);
    });
  }

  rootLayers.forEach(root => assignNewIds(root.id));

  // Update all layer nodes with new IDs
  layerNodes.forEach(layer => {
    const newId = oldToNewId.get(layer.id);
    const newParentId = layer.parent_id !== undefined ? oldToNewId.get(layer.parent_id) : undefined;
    const newChildren = layer.children.map(childId => oldToNewId.get(childId));

    layer.id = newId;
    layer.parent_id = newParentId;
    layer.children = newChildren;
  });

  // Rebuild layerById with new IDs
  layerById.clear();
  layerNodes.forEach(layer => {
    layerById.set(layer.id, layer);
  });

  return oldToNewId;
}

// Re-number to match Chromium's PaintLayer IDs
renumberLayerIds(layerNodes, layerById);

// Output statistics
console.log('='.repeat(80));
console.log('PAINT LAYER TREE COMPUTED FROM LAYOUT TREE');
console.log('='.repeat(80));
console.log();
console.log(`Total LayoutObjects: ${layoutTree.length}`);
console.log(`Total PaintLayers: ${layerNodes.length}`);
console.log(`Layers that are stacking contexts: ${layerNodes.filter(n => n.is_stacking_context).length}`);
console.log(`Layers that are stacked: ${layerNodes.filter(n => n.is_stacked).length}`);
console.log();

// Print layer tree structure
console.log('='.repeat(80));
console.log('LAYER TREE STRUCTURE');
console.log('='.repeat(80));
console.log();

function printLayerTree(layerId, indent = 0) {
  const layer = layerById.get(layerId);
  if (!layer) return;

  const prefix = '  '.repeat(indent);
  const flags = [];
  if (layer.is_stacking_context) flags.push('SC');
  if (layer.is_stacked) flags.push('STK');
  const flagStr = flags.length > 0 ? ` [${flags.join(', ')}]` : '';

  console.log(`${prefix}[${layer.id}] ${layer.name} (z: ${layer.z_index})${flagStr}`);

  layer.children.forEach(childId => {
    printLayerTree(childId, indent + 1);
  });
}

// Find and print from root
const rootLayers = layerNodes.filter(n => n.parent_id === undefined);
rootLayers.forEach(root => printLayerTree(root.id));

// Write JSON output
const outputPath = path.join(__dirname, 'computed_layer_tree.json');
const output = {
  summary: {
    total_layout_objects: layoutTree.length,
    total_layers: layerNodes.length,
    stacking_context_layers: layerNodes.filter(n => n.is_stacking_context).length,
    stacked_layers: layerNodes.filter(n => n.is_stacked).length
  },
  layer_tree: layerNodes.map(n => ({
    id: n.id,
    name: n.name,
    z_index: n.z_index,
    is_stacking_context: n.is_stacking_context,
    is_stacked: n.is_stacked,
    tag: n.tag,
    layer_depth: n.layer_depth,
    layout_depth: n.layout_depth,
    parent_id: n.parent_id !== undefined ? n.parent_id : null,
    children: n.children
  }))
};

fs.writeFileSync(outputPath, JSON.stringify(output, null, 2));

console.log();
console.log('='.repeat(80));
console.log(`Output written to: ${outputPath}`);
console.log('='.repeat(80));
