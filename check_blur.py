import json

with open("raw_paint_ops.json") as f:
    data = json.load(f)

print("=== Searching for blur-related data ===")

# Check effect tree for blur
print("\n--- Effect tree ---")
for node in data.get("effect_tree", {}).get("nodes", []):
    for key in node.keys():
        if "blur" in key.lower() or "filter" in key.lower() or "backdrop" in key.lower():
            print(f"Effect node {node['id']}: {key} = {node[key]}")

# Check paint ops for blur
print("\n--- Paint ops with blur/filter ---")
for i, op in enumerate(data.get("paint_ops", [])):
    for key in op.keys():
        if "blur" in key.lower() or "filter" in key.lower():
            print(f"Op[{i}] {op['type']}: {key} = {op[key]}")
    if op.get("flags"):
        for key in op["flags"].keys():
            if "blur" in key.lower() or "filter" in key.lower():
                print(f"Op[{i}] {op['type']} flags: {key} = {op['flags'][key]}")

# Check all effect tree keys
print("\n--- All effect tree keys ---")
all_keys = set()
for node in data.get("effect_tree", {}).get("nodes", []):
    all_keys.update(node.keys())
print(sorted(all_keys))

# Show a sample effect node
print("\n--- Sample effect nodes ---")
for node in data.get("effect_tree", {}).get("nodes", [])[:5]:
    print(node)
