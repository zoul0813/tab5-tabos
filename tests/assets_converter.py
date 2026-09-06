#!/usr/bin/env python3

from __future__ import annotations

import copy
import io
import json
import os
import struct
import sys
import tempfile
from contextlib import redirect_stderr
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from tabos_tools.assets import load_manifest, rgb565, write_c, write_header, write_tsp


def rejected(path: Path, message: str | None = None) -> bool:
    stderr = io.StringIO()
    with redirect_stderr(stderr):
        try:
            load_manifest(path)
        except SystemExit as error:
            return error.code == 2 and (message is None or message in stderr.getvalue())
    return False


def main() -> int:
    try:
        from PIL import Image
    except ImportError:
        print("Pillow unavailable", file=sys.stderr)
        return 1
    demo_map = json.loads((ROOT / "apps/tile-demo/assets/world.tmj").read_text(encoding="utf-8"))
    demo_tileset = json.loads((ROOT / "apps/tile-demo/assets/demo.tsj").read_text(encoding="utf-8"))
    if (not demo_map.get("tiledversion") or demo_map.get("nextlayerid") != 4 or
            demo_map.get("nextobjectid") != 4 or not all(layer.get("visible") for layer in demo_map["layers"]) or
            any("id" not in layer for layer in demo_map["layers"]) or
            demo_map["tilesets"] != [{"firstgid": 1, "source": "demo.tsj"}] or
            demo_tileset.get("type") != "tileset" or demo_tileset.get("image") != "sprites-64.png"):
        return 1
    demo_assets = load_manifest(ROOT / "apps/tile-demo/assets/manifest.json")
    if (len(demo_assets.images) != 1 or len(demo_assets.sprites) != 16 or
            [sprite.name for sprite in demo_assets.sprites] != [
                "grass", "water", "wall", "flowers", "robot_0", "robot_1", "robot_2", "robot_3",
                "tree", "crate", "gem", "shadow", "bush", "rock", "reeds", "dirt"] or
            (demo_assets.sprites[4].pivot_x, demo_assets.sprites[4].pivot_y) != (8, 14) or
            (demo_assets.sprites[8].pivot_x, demo_assets.sprites[8].pivot_y) != (8, 15) or
            len(demo_assets.animations) != 1 or demo_assets.animations[0].name != "robot_walk" or
            demo_assets.animations[0].frames != [(4, 120), (5, 120), (6, 120), (7, 120)] or
            len(demo_assets.metasprites) != 1):
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
        if (len(assets.images) != 2 or len(assets.animations) != 1 or assets.animations[0].name != "walk" or
                assets.animations[0].frames != [(0, 10), (1, 20)] or assets.animations[0].repeat != 3):
            return 1
        manifest["images"][0]["durations_ms"] = [3, 7]
        manifest["images"][0]["repeat_count"] = 1
        (root / "assets.json").write_text(json.dumps(manifest), encoding="utf-8")
        overridden = load_manifest(root / "assets.json")
        if overridden.animations[0].frames != [(0, 3), (1, 7)] or overridden.animations[0].repeat != 1:
            return 1
        invalid_gif_options = [
            {"durations_ms": [10]},
            {"durations_ms": "10,20"},
            {"durations_ms": [0, 20]},
            {"durations_ms": [True, 20]},
            {"repeat_count": -1},
            {"repeat_count": 0x100000000},
            {"repeat_count": True},
        ]
        for options in invalid_gif_options:
            invalid_manifest = {
                "version": 1, "name": "invalid_gif",
                "images": [{"name": "walk", "source": "animated.gif", **options}],
            }
            (root / "invalid-gif.json").write_text(json.dumps(invalid_manifest), encoding="utf-8")
            if not rejected(root / "invalid-gif.json"):
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
        opaque.save(root / "single.gif")
        single_manifest = {
            "version": 1, "name": "single", "images": [{"name": "single", "source": "single.gif"}],
        }
        (root / "single.json").write_text(json.dumps(single_manifest), encoding="utf-8")
        single_assets = load_manifest(root / "single.json")
        if (len(single_assets.images) != 1 or len(single_assets.sprites) != 1 or single_assets.animations or
                single_assets.images[0].pixels != [rgb565(255, 128, 0)]):
            return 1

        disposal_frames = []
        for x, color in ((0, (255, 0, 0, 255)), (1, (0, 255, 0, 255)), (2, (0, 0, 255, 255))):
            frame = Image.new("RGBA", (3, 1), (0, 0, 0, 0))
            frame.putpixel((x, 0), color)
            disposal_frames.append(frame)
        disposal_expectations = {
            2: [[rgb565(255, 0, 0), 0, 0], [rgb565(255, 0, 0), rgb565(0, 255, 0), 0],
                [0, 0, rgb565(0, 0, 255)]],
            3: [[rgb565(255, 0, 0), 0, 0], [rgb565(255, 0, 0), rgb565(0, 255, 0), 0],
                [rgb565(255, 0, 0), 0, rgb565(0, 0, 255)]],
        }
        for disposal, expected_pixels in disposal_expectations.items():
            gif_path = root / f"disposal-{disposal}.gif"
            disposal_frames[0].save(gif_path, save_all=True, append_images=disposal_frames[1:],
                                    duration=[20, 30, 40], loop=0, disposal=[1, disposal, 1],
                                    transparency=0, optimize=False)
            disposal_manifest = {
                "version": 1, "name": f"disposal_{disposal}",
                "images": [{"name": "effect", "source": gif_path.name}],
            }
            (root / "disposal.json").write_text(json.dumps(disposal_manifest), encoding="utf-8")
            disposal_assets = load_manifest(root / "disposal.json")
            if len(disposal_assets.animations) != 1:
                return 1
            disposal_animation = disposal_assets.animations[0]
            if (disposal_animation.name != "effect" or disposal_animation.frames != [(0, 20), (1, 30), (2, 40)] or
                    disposal_animation.repeat != 0 or [image.key for image in disposal_assets.images] != [0, 0, 0] or
                    [image.pixels for image in disposal_assets.images] != expected_pixels):
                return 1

        no_loop_path = root / "no-loop.gif"
        disposal_frames[0].save(no_loop_path, save_all=True, append_images=disposal_frames[1:],
                                duration=[20, 30, 40], disposal=[1, 1, 1], transparency=0, optimize=False)
        no_loop_manifest = {
            "version": 1, "name": "no_loop", "images": [{"name": "effect", "source": no_loop_path.name}],
        }
        (root / "no-loop.json").write_text(json.dumps(no_loop_manifest), encoding="utf-8")
        if load_manifest(root / "no-loop.json").animations[0].repeat != 1:
            return 1

        static_manifest = {"version": 1, "name": "static", "images": [{"name": "pixel", "source": "opaque.png"}]}
        (root / "static.json").write_text(json.dumps(static_manifest), encoding="utf-8")
        static_assets = load_manifest(root / "static.json")
        if static_assets.images[0].pixels != [0xFC00] or static_assets.images[0].key is not None:
            return 1
        generated = root / "generated" / "static"
        public_header = root / "include" / "static.h"
        generated.parent.mkdir()
        write_c(static_assets, generated)
        write_header(static_assets, public_header, include_declarations=False)
        generated_text = generated.with_suffix(".h").read_text(encoding="utf-8")
        public_text = public_header.read_text(encoding="utf-8")
        if (not public_text.startswith("/* Generated by tabos assets build. Do not edit. */\n") or
                "#define STATIC_SPRITE_PIXEL 0U" not in public_text or
                "_MAP_" in public_text or
                "extern const tabos_sprite_set_t static_sprites;" not in generated_text or
                "extern " in public_text or "#include <tabos/tilemap.h>" in public_text):
            return 1
        demo_header = root / "tdemo.h"
        write_header(demo_assets, demo_header, include_declarations=False)
        if demo_header.read_bytes() != (ROOT / "apps/tile-demo/include/tdemo.h").read_bytes():
            return 1

        bitmap = Image.new("RGB", (1, 1), (0, 0, 0))
        bitmap.save(root / "unsupported.bmp")
        bitmap_manifest = {
            "version": 1, "name": "bitmap",
            "images": [{"name": "bitmap", "source": "unsupported.bmp"}],
        }
        (root / "bitmap.json").write_text(json.dumps(bitmap_manifest), encoding="utf-8")
        if not rejected(root / "bitmap.json", "uses BMP format; expected GIF/PNG"):
            return 1

        malformed_manifests = [
            ([], f"asset manifest {root / 'malformed.json'} must be an object"),
            ({"version": True, "name": "bad"}, "version must be an integer"),
            ({"version": 1, "name": "bad", "images": {}}, "images must be an array"),
            ({"version": 1, "name": "bad", "maps": [1]}, "maps[0] must be an object"),
            ({"version": 1, "name": "bad", "animations": [{"frames": {}}]},
             "animation 0 frames must be an array"),
            ({"version": 1, "name": "bad", "metasprites": [{"parts": {}}]},
             "metasprite 0 parts must be an array"),
        ]
        for malformed, message in malformed_manifests:
            (root / "malformed.json").write_text(json.dumps(malformed), encoding="utf-8")
            if not rejected(root / "malformed.json", message):
                return 1

        partial = Image.new("RGBA", (1, 1), (0, 0, 0, 128))
        partial.save(root / "partial.png")
        partial_manifest = {"version": 1, "name": "partial", "images": [{"name": "pixel", "source": "partial.png"}]}
        (root / "partial.json").write_text(json.dumps(partial_manifest), encoding="utf-8")
        if not rejected(root / "partial.json"):
            return 1

        keyed = Image.new("RGBA", (2, 1))
        keyed.putdata([(0, 0, 0, 255), (7, 8, 9, 0)])
        keyed.save(root / "keyed.png")
        keyed_manifest = {
            "version": 1, "name": "keyed", "images": [{"name": "keyed", "source": "keyed.png"}],
        }
        (root / "keyed.json").write_text(json.dumps(keyed_manifest), encoding="utf-8")
        keyed_assets = load_manifest(root / "keyed.json")
        if keyed_assets.images[0].key != 1 or keyed_assets.images[0].pixels != [0, 1]:
            return 1

        collision_key_manifest = {
            "version": 1, "name": "key_collision",
            "images": [{"name": "keyed", "source": "keyed.png", "color_key": 0}],
        }
        (root / "key-collision.json").write_text(json.dumps(collision_key_manifest), encoding="utf-8")
        if not rejected(root / "key-collision.json"):
            return 1

        tolerance = Image.new("RGBA", (3, 1))
        tolerance.putdata([(100, 100, 100, 255), (109, 90, 105, 255), (111, 100, 100, 255)])
        tolerance.save(root / "tolerance.png")
        tolerance_manifest = {
            "version": 1, "name": "tolerance", "images": [{
                "name": "tolerance", "source": "tolerance.png",
                "transparent_rgb": [100, 100, 100], "transparent_tolerance": 10,
            }],
        }
        (root / "tolerance.json").write_text(json.dumps(tolerance_manifest), encoding="utf-8")
        tolerance_assets = load_manifest(root / "tolerance.json")
        if (tolerance_assets.images[0].key != 0 or
                tolerance_assets.images[0].pixels != [0, 0, rgb565(111, 100, 100)]):
            return 1

        for invalid_flags in ({"zero": 0}, {"combined": 3}, {"solid": 1, "blocking": 1}):
            invalid_flag_manifest = {
                "version": 1, "name": "invalid_flags", "flags": invalid_flags,
                "images": [{"name": "pixel", "source": "opaque.png"}],
            }
            (root / "invalid-flags.json").write_text(json.dumps(invalid_flag_manifest), encoding="utf-8")
            if not rejected(root / "invalid-flags.json"):
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
        base_tiled = copy.deepcopy(tiled)
        (root / "animated.tmj").write_text(json.dumps(tiled), encoding="utf-8")
        tiled_manifest = {"version": 1, "name": "tiled", "maps": [{"name": "world", "source": "animated.tmj"}]}
        (root / "tiled.json").write_text(json.dumps(tiled_manifest), encoding="utf-8")
        tiled_assets = load_manifest(root / "tiled.json")
        animation = tiled_assets.animations[0]
        if animation.trigger != 0 or animation.frames != [(1, 50)] or animation.repeat != 0:
            return 1
        write_tsp(tiled_assets, root / "tiled.tsp")
        tiled_data = (root / "tiled.tsp").read_bytes()
        animation_offset = struct.unpack_from("<I", tiled_data, 44)[0]
        if struct.unpack_from("<I", tiled_data, animation_offset + 12)[0] != 0:
            return 1
        tile = tiled["tilesets"][0]["tiles"][0]
        for repeat in (0, 1, 3, 0xffffffff):
            tile["properties"] = [{"name": "repeat_count", "type": "int", "value": repeat}]
            (root / "animated.tmj").write_text(json.dumps(tiled), encoding="utf-8")
            repeated = load_manifest(root / "tiled.json")
            if repeated.animations[0].repeat != repeat:
                return 1
            write_tsp(repeated, root / "tiled.tsp")
            repeated_data = (root / "tiled.tsp").read_bytes()
            offset = struct.unpack_from("<I", repeated_data, 44)[0]
            if struct.unpack_from("<I", repeated_data, offset + 8)[0] != repeat:
                return 1
        for kind, value in (("int", -1), ("int", 0x100000000), ("int", True),
                            ("int", 1.5), ("int", "1"), ("bool", True), ("string", "once")):
            tile["properties"] = [{"name": "repeat_count", "type": kind, "value": value}]
            (root / "animated.tmj").write_text(json.dumps(tiled), encoding="utf-8")
            if not rejected(root / "tiled.json"):
                return 1
        tile["properties"] = [{"name": "repeat_count", "type": "int", "value": 1}]
        del tile["animation"]
        (root / "animated.tmj").write_text(json.dumps(tiled), encoding="utf-8")
        if not rejected(root / "tiled.json"):
            return 1

        def rejects_tiled(candidate: object, message: str) -> bool:
            (root / "animated.tmj").write_text(json.dumps(candidate), encoding="utf-8")
            return rejected(root / "tiled.json", message)

        tiled_cases: list[tuple[object, str]] = []
        candidate = copy.deepcopy(base_tiled)
        candidate["infinite"] = True
        tiled_cases.append((candidate, f"Tiled map {root / 'animated.tmj'} must be finite and orthogonal"))
        candidate = copy.deepcopy(base_tiled)
        candidate["orientation"] = "isometric"
        tiled_cases.append((candidate, "must be finite and orthogonal"))
        candidate = copy.deepcopy(base_tiled)
        candidate["width"] = 0
        tiled_cases.append((candidate, "dimensions and tile dimensions must be positive"))
        candidate = copy.deepcopy(base_tiled)
        candidate["layers"] = {}
        tiled_cases.append((candidate, "layers must be an array"))
        candidate = copy.deepcopy(base_tiled)
        candidate["tilesets"] = {}
        tiled_cases.append((candidate, "tilesets must be an array"))
        candidate = copy.deepcopy(base_tiled)
        candidate["tilesets"][0]["image"] = "unsupported.bmp"
        tiled_cases.append((candidate, "uses BMP format; expected PNG"))
        candidate = copy.deepcopy(base_tiled)
        candidate["layers"][0]["data"] = [3]
        tiled_cases.append((candidate, "references unknown GID 3"))
        candidate = copy.deepcopy(base_tiled)
        candidate["layers"][0]["data"] = [0x10000001]
        tiled_cases.append((candidate, "contains malformed reserved GID bit"))
        candidate = copy.deepcopy(base_tiled)
        candidate["layers"][0]["data"] = [0x100000000]
        tiled_cases.append((candidate, "must fit an unsigned 32-bit integer"))
        candidate = copy.deepcopy(base_tiled)
        candidate["layers"][0]["offsetx"] = 1
        tiled_cases.append((candidate, "offsets are not supported"))
        candidate = copy.deepcopy(base_tiled)
        candidate["layers"] = [{"type": "imagelayer", "name": "background"}]
        tiled_cases.append((candidate, "uses unsupported layer type 'imagelayer'"))
        for candidate, message in tiled_cases:
            if not rejects_tiled(candidate, message):
                return 1

        object_base = copy.deepcopy(base_tiled)
        object_base["layers"] = [{
            "type": "objectgroup", "name": "objects",
            "objects": [{"id": 1, "name": "spawn", "point": True, "x": 0, "y": 0}],
        }]
        for unsupported_key, unsupported_value in (
                ("ellipse", True), ("text", {}), ("polygon", []), ("polyline", []), ("template", "shape.tx")):
            candidate = copy.deepcopy(object_base)
            candidate["layers"][0]["objects"][0][unsupported_key] = unsupported_value
            if not rejects_tiled(candidate, "uses unsupported geometry or rotation"):
                return 1
        candidate = copy.deepcopy(object_base)
        candidate["layers"][0]["objects"][0]["rotation"] = 90
        if not rejects_tiled(candidate, "uses unsupported geometry or rotation"):
            return 1
        candidate = copy.deepcopy(object_base)
        candidate["layers"][0]["objects"][0].update({"x": 0.5, "y": -0.5, "width": 4.6, "height": 2.4})
        (root / "animated.tmj").write_text(json.dumps(candidate), encoding="utf-8")
        warnings = io.StringIO()
        with redirect_stderr(warnings):
            rounded = load_manifest(root / "tiled.json")
        rounded_object = rounded.maps[0]["objects"][0]
        warning_text = warnings.getvalue()
        if ((rounded_object["x"], rounded_object["y"], rounded_object["width"], rounded_object["height"]) !=
                (1, -1, 5, 2) or "object 'spawn' x 0.5 rounded to 1" not in warning_text or
                "object 'spawn' y -0.5 rounded to -1" not in warning_text or
                "object 'spawn' width 4.6 rounded to 5" not in warning_text or
                "object 'spawn' height 2.4 rounded to 2" not in warning_text):
            return 1
        alignments = {
            "topleft": (100, 100), "top": (90, 100), "topright": (80, 100),
            "left": (100, 95), "center": (90, 95), "right": (80, 95),
            "bottomleft": (100, 90), "bottom": (90, 90), "bottomright": (80, 90),
            "unspecified": (100, 90),
        }
        for alignment, expected in alignments.items():
            candidate = copy.deepcopy(base_tiled)
            candidate["tilesets"][0]["objectalignment"] = alignment
            candidate["layers"] = [{
                "type": "objectgroup", "name": "objects",
                "objects": [{"id": 1, "name": "tile", "gid": 1, "x": 100, "y": 100,
                             "width": 20, "height": 10}],
            }]
            (root / "animated.tmj").write_text(json.dumps(candidate), encoding="utf-8")
            aligned_object = load_manifest(root / "tiled.json").maps[0]["objects"][0]
            if (aligned_object["x"], aligned_object["y"]) != expected:
                return 1
        candidate = copy.deepcopy(base_tiled)
        candidate["tilesets"][0]["objectalignment"] = "baseline"
        if not rejects_tiled(candidate, "objectalignment 'baseline' is not supported"):
            return 1
        candidate = copy.deepcopy(object_base)
        candidate["layers"][0]["objects"][0]["x"] = "0.5"
        if not rejects_tiled(candidate, "object 'spawn' x must be a finite number"):
            return 1
        candidate = copy.deepcopy(object_base)
        candidate["layers"][0]["objects"][0]["name"] = 4
        if not rejects_tiled(candidate, "object name must be a string"):
            return 1
        candidate = copy.deepcopy(object_base)
        candidate["layers"][0]["objects"][0]["properties"] = [
            {"name": "damage", "type": "int", "value": True},
        ]
        if not rejects_tiled(candidate, "object property must be an integer"):
            return 1
        candidate = copy.deepcopy(base_tiled)
        candidate["tilesets"][0]["tiles"][0]["properties"] = [
            {"name": "damage", "type": "string", "value": "high"},
        ]
        if not rejects_tiled(candidate, "only integer and boolean tile properties"):
            return 1
    return 0


if __name__ == "__main__":
    os.environ.setdefault("PYTHONDONTWRITEBYTECODE", "1")
    raise SystemExit(main())
