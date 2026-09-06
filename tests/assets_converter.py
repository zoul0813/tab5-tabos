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

from tabos_tools.assets import convert_pixels, load_manifest, write_c, write_header, write_tmap, write_tsp


def expected_rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def rejected(path: Path, message: str | None = None) -> bool:
    stderr = io.StringIO()
    with redirect_stderr(stderr):
        try:
            load_manifest(path)
        except SystemExit as error:
            return error.code == 2 and (message is None or message in stderr.getvalue())
    return False


def pixels_rejected(rgba: list[tuple[int, int, int, int]]) -> bool:
    stderr = io.StringIO()
    with redirect_stderr(stderr):
        try:
            convert_pixels(rgba)
        except SystemExit as error:
            return error.code == 2
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
            {"durations_ms": [0x100000000, 20]},
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
                single_assets.images[0].pixels != [expected_rgb565(255, 128, 0)]):
            return 1

        ramp = Image.new("RGBA", (256 * 3, 1))
        ramp.putdata(
            [(value, 0, 0, 255) for value in range(256)] +
            [(0, value, 0, 255) for value in range(256)] +
            [(0, 0, value, 255) for value in range(256)])
        ramp.save(root / "rgb565-ramp.png")
        ramp_manifest = {
            "version": 1, "name": "rgb565_ramp",
            "images": [{"name": "ramp", "source": "rgb565-ramp.png"}],
        }
        (root / "rgb565-ramp.json").write_text(json.dumps(ramp_manifest), encoding="utf-8")
        ramp_assets = load_manifest(root / "rgb565-ramp.json")
        expected_ramp = (
            [expected_rgb565(value, 0, 0) for value in range(256)] +
            [expected_rgb565(0, value, 0) for value in range(256)] +
            [expected_rgb565(0, 0, value) for value in range(256)])
        if ramp_assets.images[0].pixels != expected_ramp:
            return 1

        disposal_frames = []
        for x, color in ((0, (255, 0, 0, 255)), (1, (0, 255, 0, 255)), (2, (0, 0, 255, 255))):
            frame = Image.new("RGBA", (3, 1), (0, 0, 0, 0))
            frame.putpixel((x, 0), color)
            disposal_frames.append(frame)
        disposal_expectations = {
            2: [[expected_rgb565(255, 0, 0), 0, 0],
                [expected_rgb565(255, 0, 0), expected_rgb565(0, 255, 0), 0],
                [0, 0, expected_rgb565(0, 0, 255)]],
            3: [[expected_rgb565(255, 0, 0), 0, 0],
                [expected_rgb565(255, 0, 0), expected_rgb565(0, 255, 0), 0],
                [expected_rgb565(255, 0, 0), 0, expected_rgb565(0, 0, 255)]],
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

        deterministic_outputs = []
        for directory_name in ("deterministic-one", "deterministic-two"):
            output = root / directory_name
            output.mkdir()
            generated_assets = copy.deepcopy(demo_assets)
            original_assets = copy.deepcopy(generated_assets)
            stem = output / "tdemo"
            write_tsp(generated_assets, stem.with_suffix(".tsp"))
            for tiled_map in generated_assets.maps:
                write_tmap(tiled_map, output / f"{tiled_map['name']}.tmap")
            write_c(generated_assets, stem)
            if generated_assets != original_assets:
                return 1
            deterministic_outputs.append(output)
        for filename in ("tdemo.c", "tdemo.h", "tdemo.tsp", "world.tmap"):
            if ((deterministic_outputs[0] / filename).read_bytes() !=
                    (deterministic_outputs[1] / filename).read_bytes()):
                return 1
        tsp_header = (deterministic_outputs[0] / "tdemo.tsp").read_bytes()[:12]
        tmap_header = (deterministic_outputs[0] / "world.tmap").read_bytes()[:12]
        if (struct.unpack("<4s2I", tsp_header)[:2] != (b"TSP1", 1) or
                struct.unpack("<4s2I", tmap_header)[:2] != (b"TMP1", 1)):
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
        if any(not pixels_rejected([(1, 2, 3, alpha)]) for alpha in range(1, 255)):
            return 1
        partial_manifest["images"][0]["transparent_rgb"] = [0, 0, 0]
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

        sheet = Image.new("RGBA", (5, 4), (12, 34, 56, 255))
        sheet.save(root / "sheet.png")
        authored_manifest = {
            "version": 1,
            "name": "authored",
            "flags": {"solid": 1, "hazard": 4},
            "images": [{
                "name": "sheet",
                "source": "sheet.png",
                "sprites": [
                    {"name": "hero", "x": 1, "y": 1, "width": 2, "height": 2,
                     "pivot": [1, 2], "flags": ["solid", "hazard"]},
                    {"name": "attachment", "x": 0, "y": 0, "width": 1, "height": 1,
                     "pivot": [-3, 7], "flags": 0x80000000},
                    {"name": "edge", "x": 4, "y": 3, "width": 1, "height": 1,
                     "pivot": [1, 1], "flags": []},
                ],
            }],
            "animations": [
                {"name": "loop", "frames": [
                    {"sprite": "hero", "duration_ms": 7},
                    {"sprite": "attachment", "duration_ms": 9},
                ]},
                {"name": "flash", "repeat_count": 3,
                 "frames": [{"sprite": "edge", "duration_ms": 11}]},
            ],
            "metasprites": [{"name": "assembled", "parts": [
                {"sprite": "hero", "x": -8, "y": 9, "rotation": 0,
                 "mirror_x": False, "mirror_y": False, "opacity": 255},
                {"sprite": "attachment", "x": 6, "y": -7, "rotation": 1,
                 "mirror_x": True, "mirror_y": False, "opacity": 128},
                {"sprite": "edge", "rotation": 2, "mirror_x": False,
                 "mirror_y": True, "opacity": 0},
                {"sprite": "hero", "rotation": 3, "mirror_x": True,
                 "mirror_y": True},
            ]}],
        }
        (root / "authored.json").write_text(json.dumps(authored_manifest), encoding="utf-8")
        authored = load_manifest(root / "authored.json")
        sprite_values = [
            (item.name, item.image, item.x, item.y, item.width, item.height,
             item.pivot_x, item.pivot_y, item.flags)
            for item in authored.sprites
        ]
        if (len(authored.images) != 1 or sprite_values != [
                ("hero", 0, 1, 1, 2, 2, 1, 2, 5),
                ("attachment", 0, 0, 0, 1, 1, -3, 7, 0x80000000),
                ("edge", 0, 4, 3, 1, 1, 1, 1, 0),
            ] or
            [(item.name, item.frames, item.repeat) for item in authored.animations] != [
                ("loop", [(0, 7), (1, 9)], 0), ("flash", [(2, 11)], 3),
            ] or
            authored.metasprites[0].parts != [
                {"sprite": 0, "x": -8, "y": 9, "rotation": 0,
                 "mirror_x": False, "mirror_y": False, "opacity": 255},
                {"sprite": 1, "x": 6, "y": -7, "rotation": 1,
                 "mirror_x": True, "mirror_y": False, "opacity": 128},
                {"sprite": 2, "x": 0, "y": 0, "rotation": 2,
                 "mirror_x": False, "mirror_y": True, "opacity": 0},
                {"sprite": 0, "x": 0, "y": 0, "rotation": 3,
                 "mirror_x": True, "mirror_y": True, "opacity": 255},
            ]):
            return 1
        authored_header = root / "authored.h"
        write_header(authored, authored_header, include_declarations=False)
        authored_header_text = authored_header.read_text(encoding="utf-8")
        for definition in (
                "#define AUTHORED_FLAG_SOLID 1U",
                "#define AUTHORED_FLAG_HAZARD 4U",
                "#define AUTHORED_SPRITE_HERO 0U",
                "#define AUTHORED_SPRITE_ATTACHMENT 1U",
                "#define AUTHORED_ANIMATION_LOOP 0U",
                "#define AUTHORED_ANIMATION_FLASH 1U",
                "#define AUTHORED_METASPRITE_ASSEMBLED 0U"):
            if definition not in authored_header_text:
                return 1

        invalid_authored_manifests = [
            {"images": [{"name": "sheet", "source": "sheet.png", "sprites": {}}]},
            {"images": [{"name": "sheet", "source": "sheet.png", "sprites": [1]}]},
            {"images": [{"name": "sheet", "source": "sheet.png",
                         "sprites": [{"name": "bad", "x": -1}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png",
                         "sprites": [{"name": "bad", "width": 0}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png",
                         "sprites": [{"name": "bad", "x": 4, "width": 2}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png",
                         "sprites": [{"name": "bad", "pivot": [0]}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png",
                         "sprites": [{"name": "bad", "pivot": [0x80000000, 0]}]}]},
            {"flags": {"solid": 1}, "images": [{"name": "sheet", "source": "sheet.png",
                         "sprites": [{"name": "bad", "flags": ["missing"]}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "animations": [{"name": "bad", "frames": []}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "animations": [{"name": "bad", "frames": [{"sprite": "missing", "duration_ms": 1}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "animations": [{"name": "bad", "frames": [{"sprite": "sheet", "duration_ms": True}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "animations": [{"name": "bad", "frames": [{"sprite": "sheet", "duration_ms": 0}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "animations": [{"name": "bad", "frames": [
                 {"sprite": "sheet", "duration_ms": 0x100000000}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "animations": [{"name": "bad", "repeat_count": -1,
                              "frames": [{"sprite": "sheet", "duration_ms": 1}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "animations": [{"name": "bad", "repeat_count": True,
                              "frames": [{"sprite": "sheet", "duration_ms": 1}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "metasprites": [{"name": "bad", "parts": []}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "metasprites": [{"name": "bad", "parts": [{"sprite": "missing"}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "metasprites": [{"name": "bad", "parts": [{"sprite": "sheet", "rotation": 4}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "metasprites": [{"name": "bad", "parts": [{"sprite": "sheet", "opacity": 256}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "metasprites": [{"name": "bad", "parts": [{"sprite": "sheet", "mirror_x": 1}]}]},
            {"images": [{"name": "sheet", "source": "sheet.png"}],
             "metasprites": [{"name": "bad", "parts": [{"sprite": "sheet", "x": -0x80000001}]}]},
        ]
        for invalid_index, invalid_sections in enumerate(invalid_authored_manifests):
            invalid_manifest = {"version": 1, "name": "invalid_authored", **invalid_sections}
            invalid_path = root / f"invalid-authored-{invalid_index}.json"
            invalid_path.write_text(json.dumps(invalid_manifest), encoding="utf-8")
            if not rejected(invalid_path):
                return 1

        multi_image_manifest = {
            "version": 1, "name": "multi_image",
            "images": [{"name": "pixel", "source": "opaque.png"},
                       {"name": "keyed", "source": "keyed.png"}],
            "animations": [{"name": "mixed", "frames": [
                {"sprite": "pixel", "duration_ms": 4}, {"sprite": "keyed", "duration_ms": 6},
            ]}],
            "metasprites": [{"name": "mixed", "parts": [
                {"sprite": "pixel"}, {"sprite": "keyed", "x": 1},
            ]}],
        }
        (root / "multi-image.json").write_text(json.dumps(multi_image_manifest), encoding="utf-8")
        multi_image = load_manifest(root / "multi-image.json")
        if (len(multi_image.images) != 2 or [sprite.image for sprite in multi_image.sprites] != [0, 1] or
                [image.key for image in multi_image.images] != [None, 1] or
                multi_image.animations[0].frames != [(0, 4), (1, 6)] or
                [part["sprite"] for part in multi_image.metasprites[0].parts] != [0, 1]):
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
                tolerance_assets.images[0].pixels != [0, 0, expected_rgb565(111, 100, 100)]):
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

        external_tileset = copy.deepcopy(base_tiled["tilesets"][0])
        del external_tileset["firstgid"]
        external_tileset["type"] = "tileset"
        (root / "external.tsj").write_text(json.dumps(external_tileset), encoding="utf-8")
        external_map = copy.deepcopy(base_tiled)
        external_map["tilesets"] = [{"firstgid": 1, "source": "external.tsj"}]
        (root / "external.tmj").write_text(json.dumps(external_map), encoding="utf-8")
        external_manifest = {
            "version": 1, "name": "tiled", "maps": [{"name": "world", "source": "external.tmj"}],
        }
        (root / "external.json").write_text(json.dumps(external_manifest), encoding="utf-8")
        if load_manifest(root / "external.json") != tiled_assets:
            return 1

        duplicate_manifest_image = {
            "version": 1, "name": "duplicate",
            "images": [{"name": "standalone", "source": "tiles.png"}],
            "maps": [{"name": "world", "source": "animated.tmj"}],
        }
        (root / "duplicate-tiled-image.json").write_text(json.dumps(duplicate_manifest_image), encoding="utf-8")
        if not rejected(root / "duplicate-tiled-image.json", "already imported by the manifest"):
            return 1
        duplicate_manifest_image["maps"] = []
        duplicate_manifest_image["images"].append({"name": "again", "source": "tiles.png"})
        (root / "duplicate-image.json").write_text(json.dumps(duplicate_manifest_image), encoding="utf-8")
        if not rejected(root / "duplicate-image.json", "duplicates source from images[0]"):
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
        del candidate["tilesets"][0]["image"]
        tiled_cases.append((candidate, "collection-of-images tilesets are not supported"))
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

        candidate = copy.deepcopy(base_tiled)
        candidate["tilesets"][0]["tiles"][0]["animation"][0]["duration"] = 5
        (root / "animated.tmj").write_text(json.dumps(candidate), encoding="utf-8")
        precise_animation = load_manifest(root / "tiled.json").animations[0]
        if precise_animation.frames != [(1, 5)]:
            return 1

        invalid_animations = [
            ({}, "animation must be an array"),
            ([], "animation needs at least one frame"),
            ([{"tileid": 2, "duration": 10}], "references invalid tile"),
            ([{"tileid": True, "duration": 10}], "animation tileid must be an integer"),
            ([{"tileid": 1, "duration": 0}], "animation duration must be positive"),
            ([{"tileid": 1, "duration": True}], "animation duration must be an integer"),
        ]
        for animation_value, message in invalid_animations:
            candidate = copy.deepcopy(base_tiled)
            candidate["tilesets"][0]["tiles"][0]["animation"] = animation_value
            if not rejects_tiled(candidate, message):
                return 1

        candidate = copy.deepcopy(base_tiled)
        overlapping = copy.deepcopy(candidate["tilesets"][0])
        overlapping["firstgid"] = 2
        overlapping["name"] = "overlapping"
        candidate["tilesets"].append(overlapping)
        if not rejects_tiled(candidate, "GID 2 overlaps another tileset"):
            return 1

        candidate = copy.deepcopy(base_tiled)
        candidate["tilesets"][0]["tiles"].append(copy.deepcopy(candidate["tilesets"][0]["tiles"][0]))
        if not rejects_tiled(candidate, "defines tile id 0 more than once"):
            return 1

        candidate = copy.deepcopy(base_tiled)
        candidate["tilesets"][0]["tiles"][0]["properties"] = [
            {"name": "name", "type": "string", "value": "duplicate"},
        ]
        candidate["tilesets"][0]["tiles"].append({
            "id": 1, "properties": [{"name": "name", "type": "string", "value": "duplicate"}],
        })
        if not rejects_tiled(candidate, "asset identifier collision"):
            return 1

        candidate = copy.deepcopy(base_tiled)
        candidate["tilesets"][0]["tiles"][0]["properties"] = [
            {"name": "pivot_x", "type": "string", "value": "center"},
        ]
        if not rejects_tiled(candidate, "pivot_x must be an integer property"):
            return 1
    return 0


if __name__ == "__main__":
    os.environ.setdefault("PYTHONDONTWRITEBYTECODE", "1")
    raise SystemExit(main())
