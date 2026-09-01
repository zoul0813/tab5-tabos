#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from tabos_tools.assets import load_manifest, write_tsp


def rejected(path: Path) -> bool:
    try:
        load_manifest(path)
    except SystemExit as error:
        return error.code == 2
    return False


def main() -> int:
    try:
        from PIL import Image
    except ImportError:
        print("Pillow unavailable", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        first = Image.new("RGBA", (2, 2), (255, 0, 0, 255))
        first.putpixel((0, 0), (0, 0, 0, 0))
        second = Image.new("RGBA", (2, 2), (0, 255, 0, 255))
        first.save(root / "animated.gif", save_all=True, append_images=[second], duration=[1, 20], loop=2,
                   disposal=[2, 1], transparency=0)
        manifest = {"version": 1, "name": "fixture", "images": [{"name": "walk", "source": "animated.gif"}]}
        (root / "assets.json").write_text(json.dumps(manifest), encoding="utf-8")
        assets = load_manifest(root / "assets.json")
        if len(assets.images) != 2 or len(assets.animations) != 1 or assets.animations[0].frames[0][1] != 10 or assets.animations[0].repeat != 3:
            return 1
        write_tsp(assets, root / "one.tsp")
        write_tsp(assets, root / "two.tsp")
        if (root / "one.tsp").read_bytes() != (root / "two.tsp").read_bytes():
            return 1
        data = (root / "one.tsp").read_bytes()
        if data[:4] != b"TSP1" or struct.unpack_from("<I", data, 8)[0] != len(data):
            return 1

        opaque = Image.new("RGBA", (1, 1), (255, 128, 0, 255))
        opaque.save(root / "opaque.png")
        static_manifest = {"version": 1, "name": "static", "images": [{"name": "pixel", "source": "opaque.png"}]}
        (root / "static.json").write_text(json.dumps(static_manifest), encoding="utf-8")
        static_assets = load_manifest(root / "static.json")
        if static_assets.images[0].pixels != [0xFC00] or static_assets.images[0].key is not None:
            return 1

        partial = Image.new("RGBA", (1, 1), (0, 0, 0, 128))
        partial.save(root / "partial.png")
        partial_manifest = {"version": 1, "name": "partial", "images": [{"name": "pixel", "source": "partial.png"}]}
        (root / "partial.json").write_text(json.dumps(partial_manifest), encoding="utf-8")
        if not rejected(root / "partial.json"):
            return 1

        collision_manifest = {"version": 1, "name": "collision", "images": [{"name": "sheet", "source": "opaque.png",
            "sprites": [{"name": "foo-bar"}, {"name": "foo_bar"}]}]}
        (root / "collision.json").write_text(json.dumps(collision_manifest), encoding="utf-8")
        if not rejected(root / "collision.json"):
            return 1

        tiles = Image.new("RGBA", (2, 1), (255, 0, 0, 255))
        tiles.putpixel((1, 0), (0, 255, 0, 255))
        tiles.save(root / "tiles.png")
        tiled = {
            "type": "map", "version": "1.10", "orientation": "orthogonal", "infinite": False,
            "width": 1, "height": 1, "tilewidth": 1, "tileheight": 1,
            "tilesets": [{"firstgid": 1, "name": "tiles", "image": "tiles.png", "imagewidth": 2,
                          "imageheight": 1, "tilewidth": 1, "tileheight": 1, "columns": 2, "tilecount": 2,
                          "tiles": [{"id": 0, "animation": [{"tileid": 1, "duration": 50}]}]}],
            "layers": [{"type": "tilelayer", "name": "ground", "width": 1, "height": 1, "data": [1]}],
        }
        (root / "animated.tmj").write_text(json.dumps(tiled), encoding="utf-8")
        tiled_manifest = {"version": 1, "name": "tiled", "maps": [{"name": "world", "source": "animated.tmj"}]}
        (root / "tiled.json").write_text(json.dumps(tiled_manifest), encoding="utf-8")
        tiled_assets = load_manifest(root / "tiled.json")
        animation = tiled_assets.animations[0]
        if animation.trigger != 0 or animation.frames != [(1, 50)]:
            return 1
        write_tsp(tiled_assets, root / "tiled.tsp")
        tiled_data = (root / "tiled.tsp").read_bytes()
        animation_offset = struct.unpack_from("<I", tiled_data, 44)[0]
        if struct.unpack_from("<I", tiled_data, animation_offset + 12)[0] != 0:
            return 1
    return 0


if __name__ == "__main__":
    os.environ.setdefault("PYTHONDONTWRITEBYTECODE", "1")
    raise SystemExit(main())
