"""Author a tiny PNG/Tiled fixture; the normal converter produces both runtime forms."""

import json
import sys
from pathlib import Path

from PIL import Image


def write_json(root, name, value):
    (root / name).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def prop(name, value, kind="int"):
    return {"name": name, "type": kind, "value": value}


def main():
    root = Path(sys.argv[1])
    root.mkdir(parents=True, exist_ok=True)
    atlas = Image.new("RGBA", (4, 2))
    atlas.putdata([
        (255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 0, 0), (255, 255, 0, 255),
        (0, 0, 255, 255), (255, 255, 255, 255), (255, 255, 0, 255), (255, 255, 0, 255),
    ])
    atlas.save(root / "atlas.png")
    Image.new("RGBA", (2, 2), (0, 255, 255, 255)).save(root / "accent.png")
    write_json(root, "atlas.tsj", {
        "name": "atlas", "image": "atlas.png", "tilewidth": 2, "tileheight": 2,
        "columns": 2, "tilecount": 2,
        "tiles": [
            {"id": 0, "properties": [prop("name", "quad", "string"), prop("pivot_x", 1),
                                     prop("pivot_y", 1), prop("solid", True, "bool")]},
            {"id": 1, "properties": [prop("name", "keyed", "string"),
                                     prop("animation_name", "cycle", "string")],
             "animation": [{"tileid": 0, "duration": 10}, {"tileid": 1, "duration": 20}]},
        ],
    })
    write_json(root, "accent.tsj", {
        "name": "accent", "image": "accent.png", "tilewidth": 2, "tileheight": 2,
        "columns": 1, "tilecount": 1,
        "tiles": [{"id": 0, "properties": [prop("name", "accent", "string"), prop("water", 1)]}],
    })
    transforms = [0, 0x80000000, 0x40000000, 0xc0000000,
                  0x20000000, 0xa0000000, 0x60000000, 0xe0000000]
    write_json(root, "world.tmj", {
        "orientation": "orthogonal", "infinite": False, "width": 8, "height": 2,
        "tilewidth": 2, "tileheight": 2,
        "tilesets": [{"firstgid": 1, "source": "atlas.tsj"}, {"firstgid": 19, "source": "accent.tsj"}],
        "layers": [
            {"type": "tilelayer", "name": "ground", "width": 8, "height": 2,
             "data": [1 | flags for flags in transforms] + [2, 0, 19, 0, 2, 0, 19, 0]},
            {"type": "objectgroup", "name": "markers", "objects": [
                {"id": 7, "name": "spawn", "class": "actor", "point": True, "x": -2, "y": 3,
                 "properties": [prop("health", 23), prop("minimum", -2147483648)]},
                {"id": 11, "name": "zone", "type": "area", "x": 1, "y": -3, "width": 4, "height": 2},
            ]},
            {"type": "tilelayer", "name": "front", "width": 8, "height": 2,
             "data": [0, 2] + [0] * 14},
            {"type": "objectgroup", "name": "items", "objects": [
                {"id": 29, "name": "item", "class": "pickup", "gid": 19 | 0xa0000000,
                 "x": 6, "y": 4, "width": 2, "height": 2, "properties": [prop("maximum", 2147483647)]},
            ]},
        ],
    })
    write_json(root, "manifest.json", {
        "version": 1, "name": "equivalence", "flags": {"solid": 0x80000000, "water": 2},
        "maps": [{"name": "world", "source": "world.tmj"}],
        "animations": [{"name": "hold", "repeat_count": 2, "frames": [
            {"sprite": "accent", "duration_ms": 7}, {"sprite": "keyed", "duration_ms": 13}]}],
        "metasprites": [{"name": "actor", "parts": [
            {"sprite": "quad", "x": -1, "y": 1, "rotation": 1, "mirror_x": True, "opacity": 128},
            {"sprite": "accent", "x": -1, "y": 2, "mirror_y": True, "opacity": 255}]}],
    })


if __name__ == "__main__":
    main()
