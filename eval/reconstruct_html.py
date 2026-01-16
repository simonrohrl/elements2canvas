#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import os
import re
from dataclasses import dataclass, field
from typing import Iterable, Iterator, List, Optional, Tuple


VOID_ELEMENTS = {
    "area",
    "base",
    "br",
    "col",
    "embed",
    "hr",
    "img",
    "input",
    "link",
    "meta",
    "param",
    "source",
    "track",
    "wbr",
}


@dataclass
class Node:
    kind: str
    name: Optional[str] = None
    attrs: str = ""
    value: Optional[str] = None
    children: List["Node"] = field(default_factory=list)

    def add_child(self, child: "Node") -> None:
        self.children.append(child)


def extract_timestamp(header: str) -> Optional[str]:
    match = re.search(r"\[(?:[^:\]]+:){2}([^:\]]+):", header)
    if not match:
        return None
    field = match.group(1)
    if "/" in field:
        _, field = field.split("/", 1)
    return field or None


def iter_frame_blocks(log_path: str) -> Iterator[Tuple[str, Optional[str], List[str]]]:
    header: Optional[str] = None
    timestamp: Optional[str] = None
    current: List[str] = []
    with open(log_path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.rstrip("\n")
            if line.startswith("[") and "third_party/blink/renderer/core/dom/node.cc" in line:
                if header is not None and current:
                    yield header, timestamp, current
                header = line.strip()
                timestamp = extract_timestamp(header)
                current = []
                continue
            if header is None:
                continue
            if not line.strip():
                continue
            current.append(line)
    if header is not None and current:
        yield header, timestamp, current


def count_indent(line: str) -> int:
    count = 0
    for ch in line:
        if ch == "\t":
            count += 1
        else:
            break
    return count


def parse_text_literal(raw: str) -> str:
    raw = raw.strip()
    if not raw:
        return ""
    if raw.startswith('"') and raw.endswith('"'):
        try:
            return ast.literal_eval(raw)
        except Exception:
            return raw.strip('"')
    return raw


def create_node(content: str) -> Optional[Node]:
    if not content:
        return None
    if content.startswith("*"):
        content = content[1:].lstrip()
    if not content:
        return None
    if content.startswith("#document"):
        return Node(kind="document")
    if content.startswith("DOCTYPE"):
        name = content[len("DOCTYPE") :].strip() or "html"
        return Node(kind="doctype", value=name)
    if content.startswith("#text"):
        payload = content[len("#text") :].strip()
        return Node(kind="text", value=parse_text_literal(payload))
    if content.startswith("#comment"):
        payload = content[len("#comment") :].strip()
        comment = parse_text_literal(payload) if payload else ""
        return Node(kind="comment", value=comment)
    if content.startswith("#shadow-root"):
        mode = content[len("#shadow-root") :].strip()
        if mode.lower().startswith("(useragent") or mode.lower().startswith("(user-agent"):
            node = Node(kind="shadow-root", value=mode, children=[])
            node._skip_children = True  # type: ignore[attr-defined]
            return node
        return Node(kind="shadow-root", value=mode)
    if content.startswith("#"):
        # Unknown node type, keep as comment to avoid data loss.
        return Node(kind="comment", value=content)
    if " " in content:
        name, attrs = content.split(" ", 1)
    else:
        name, attrs = content, ""
    return Node(kind="element", name=name, attrs=attrs.strip())


def parse_frame(lines: List[str]) -> Optional[Node]:
    root = Node(kind="root")
    stack: List[Tuple[int, Node]] = [(-1, root)]
    for line in lines:
        depth = count_indent(line)
        content = line.lstrip("\t")
        if content.startswith("::"):
            continue
        node = create_node(content)
        if node is None:
            continue
        while stack and depth <= stack[-1][0]:
            stack.pop()
        parent = stack[-1][1]
        skip_parent = getattr(parent, "_skip_children", False)
        if skip_parent:
            continue
        parent.add_child(node)
        stack.append((depth, node))
    # Prefer returning the first child (typically #document)
    if root.children:
        return root.children[0]
    return None


def render_node(node: Node, indent: int = 0) -> str:
    indent_str = "  " * indent
    if node.kind in {"root", "document"}:
        return "".join(render_node(child, indent) for child in node.children)
    if node.kind == "doctype":
        name = node.value or "html"
        return f"<!DOCTYPE {name}>\n"
    if node.kind == "element":
        tag = (node.name or "").lower()
        attrs = f" {node.attrs}" if node.attrs else ""
        if tag in VOID_ELEMENTS and not node.children:
            return f"{indent_str}<{tag}{attrs}>\n"
        if len(node.children) == 1 and node.children[0].kind == "text":
            text = node.children[0].value or ""
            if "\n" not in text:
                return f"{indent_str}<{tag}{attrs}>{text}</{tag}>\n"
        result = f"{indent_str}<{tag}{attrs}>\n"
        for child in node.children:
            result += render_node(child, indent + 1)
        result += f"{indent_str}</{tag}>\n"
        return result
    if node.kind == "text":
        text = node.value or ""
        if not text:
            return ""
        if text.strip() == "":
            return text if text.endswith("\n") else text + "\n"
        if text.startswith("\n"):
            return text if text.endswith("\n") else text + "\n"
        return f"{indent_str}{text}\n"
    if node.kind == "comment":
        comment = node.value or ""
        if comment:
            return f"{indent_str}<!-- {comment} -->\n"
        return f"{indent_str}<!-- -->\n"
    if node.kind == "shadow-root":
        mode = node.value or ""
        header = f"{indent_str}<!-- #shadow-root{mode} -->\n"
        body = "".join(render_node(child, indent + 1) for child in node.children)
        return header + body
    return ""


def build_frame_html(node: Optional[Node], header: str, timestamp: Optional[str]) -> str:
    if node is None:
        return ""
    rendered = render_node(node, indent=0)
    if not rendered.endswith("\n"):
        rendered += "\n"
    label = timestamp or ""
    if header:
        if label:
            header_comment = f"<!-- Frame {label}: {header} -->\n"
        else:
            header_comment = f"<!-- Frame: {header} -->\n"
    else:
        header_comment = ""
    return header_comment + rendered


def sanitize_timestamp(value: str) -> str:
    sanitized = value.replace("/", "_").replace(":", "_")
    sanitized = re.sub(r"[^A-Za-z0-9._-]", "_", sanitized)
    return sanitized.strip("_") or "frame"


def strip_pseudo_before_after(node: Node) -> bool:
    if node.kind == "comment" and node.value:
        lowered = node.value.strip().lower()
        if "::before" in lowered or "::after" in lowered:
            return True
    return False


def reconstruct_frames(log_path: str, out_dir: str) -> int:
    os.makedirs(out_dir, exist_ok=True)
    used_names: set[str] = set()
    count = 0
    for idx, (header, timestamp, lines) in enumerate(iter_frame_blocks(log_path), start=1):
        node = parse_frame(lines)
        html = build_frame_html(node, header, timestamp)
        if not html:
            continue
        base_name = sanitize_timestamp(timestamp or f"frame_{idx:05d}")
        unique_name = base_name
        suffix = 1
        while unique_name in used_names:
            unique_name = f"{base_name}_{suffix}"
            suffix += 1
        used_names.add(unique_name)
        out_path = os.path.join(out_dir, f"{unique_name}.html")
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(html)
        count += 1
    return count


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Reconstruct HTML frames from chrome_debug.log dumps."
    )
    parser.add_argument(
        "--log",
        default="chrome_debug.log",
        help="Path to chrome_debug.log (default: chrome_debug.log in current directory).",
    )
    parser.add_argument(
        "--out-dir",
        default="frames",
        help="Output directory for reconstructed HTML frames (default: frames).",
    )

    args = parser.parse_args()
    log_path = os.path.abspath(args.log)
    out_dir = os.path.abspath(args.out_dir)

    if not os.path.exists(log_path):
        raise FileNotFoundError(f"Debug log not found: {log_path}")

    frame_count = reconstruct_frames(log_path, out_dir)
    print(f"Wrote {frame_count} frame(s) to {out_dir}")


if __name__ == "__main__":
    main()

