#!/usr/bin/env python3
"""
WebSocket server for layout tree extraction.

Receives layout_tree JSON from browser and saves to layout_tree_js.json

Usage:
    python layout_server.py

Then run ./scripts/start.sh to launch Chromium with sample_layout.html.
The page will automatically send the layout tree after 1 second.
"""

import asyncio
import json
from pathlib import Path
from datetime import datetime

try:
    import websockets
except ImportError:
    print("Error: websockets library not installed")
    print("Install with: pip install websockets")
    exit(1)

# Configuration
WS_PORT = 8765
OUTPUT_DIR = Path(__file__).parent
OUTPUT_FILE = OUTPUT_DIR / "layout_tree_js.json"

# Global state
last_received_time = None


async def handle_websocket(websocket):
    """Handle WebSocket connections from browser."""
    global last_received_time

    client_addr = websocket.remote_address
    print(f"[WS] Client connected: {client_addr}")

    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                msg_type = data.get("type")

                if msg_type == "layout_tree":
                    layout_tree = data.get("data")
                    node_count = len(layout_tree.get("layout_tree", []))

                    with open(OUTPUT_FILE, "w") as f:
                        json.dump(layout_tree, f, indent=2)

                    last_received_time = datetime.now()

                    print(f"[WS] Received layout tree with {node_count} nodes")
                    print(f"[WS] Saved to {OUTPUT_FILE}")

                    await websocket.send(json.dumps({
                        "type": "ack",
                        "message": f"Saved {node_count} nodes"
                    }))
                else:
                    print(f"[WS] Unknown message type: {msg_type}")

            except json.JSONDecodeError as e:
                print(f"[WS] Invalid JSON: {e}")

    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        print(f"[WS] Client disconnected: {client_addr}")


async def main():
    """Main entry point."""
    print("=" * 60)
    print("Layout Tree Extraction Server")
    print("=" * 60)
    print(f"[WS] WebSocket server on ws://localhost:{WS_PORT}")
    print(f"[WS] Output file: {OUTPUT_FILE}")
    print()
    print("Run ./scripts/start.sh to launch Chromium")
    print("=" * 60)

    async with websockets.serve(handle_websocket, "localhost", WS_PORT, max_size=50 * 1024 * 1024):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[Server] Shutting down...")
