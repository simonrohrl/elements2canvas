Generate property_trees.json
Log the property trees (transform, clip, effect) from Chromium before paint traversal, treating them as input data.

Background
Per the paint system plan, the paint system requires property_trees.json as an input. The property trees are built during the pre-paint phase and are immutable during paint. We should log them before the paint traversal starts.

The existing logging in 
paint_layer_painter.cc
 (lines 625-677) logs:

LAYOUT_TREE: {...} - LayoutObject tree
LAYER_TREE: {...} - PaintLayer tree
PAINT_ORDER: {...} - Stacking nodes with z-order lists
We will add:

PROPERTY_TREES: {...} - Transform/Clip/Effect trees
[Platform] Shared Property Tree Serializer
Create a new shared serializer in platform/graphics/paint/ to avoid code duplication and layering violations. This file will handle the serialization of property nodes, while the collection of nodes remains in the caller files.

[NEW] 
property_tree_serializer.h
Defines PropertyTreeIdMapper class (exported).
Declares 
SerializeTransformTree
, 
SerializeClipTree
, 
SerializeEffectTree
 (exported).
[NEW] 
property_tree_serializer.cc
Implements the mapper and serialization functions.
[Platform] Paint Artifact Serializer
Refactor 
paint_artifact_serializer.cc
 to use the shared serializer.

[MODIFY] 
paint_artifact_serializer.cc
Remove internal PropertyTreeIdMapper and tree serialization functions.
Include 
property_tree_serializer.h
.
Update 
SerializeRawPaintOps
 to use the shared functions.
[Core] Paint Layer Painter
Update 
paint_layer_painter.cc
 to use the shared serializer for logging property trees.

[MODIFY] 
paint_layer_painter.cc
Remove internal PropertyTreeIdMapper and duplicated serialization logic.
Include 
platform/graphics/paint/property_tree_serializer.h
.
Collect nodes from fragments (pre-paint logic).
Use PropertyTreeIdMapper and shared Serialize...Tree functions.
Verification Plan
Automated Tests
Build Chromium and run with test page

autoninja -C out/Default chrome
./out/Default/Chromium.app/.../Chromium --enable-logging=stderr 2>&1 | grep PROPERTY_TREES
Verify JSON structure

Should contain transform_tree, clip_tree, effect_tree arrays
Each node should have 
id
, parent_id, and type-specific properties
Node IDs should be consistent with those used in paint ops