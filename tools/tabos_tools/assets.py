from __future__ import annotations

import argparse
import json
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .common import fail


GID_HORIZONTAL = 0x80000000
GID_VERTICAL = 0x40000000
GID_DIAGONAL = 0x20000000
GID_RESERVED = 0x10000000
GID_MASK = 0x0FFFFFFF


@dataclass
class Image:
    name: str
    width: int
    height: int
    pixels: list[int]
    key: int | None


@dataclass
class Sprite:
    name: str
    image: int
    x: int
    y: int
    width: int
    height: int
    pivot_x: int = 0
    pivot_y: int = 0
    flags: int = 0


@dataclass
class Animation:
    name: str
    frames: list[tuple[int, int]]
    repeat: int = 0
    trigger: int | None = None


@dataclass
class Metasprite:
    name: str
    parts: list[dict[str, int]]


@dataclass
class AssetSet:
    name: str
    images: list[Image] = field(default_factory=list)
    sprites: list[Sprite] = field(default_factory=list)
    animations: list[Animation] = field(default_factory=list)
    metasprites: list[Metasprite] = field(default_factory=list)
    maps: list[dict[str, Any]] = field(default_factory=list)
    constants: dict[str, list[str]] = field(default_factory=dict)
    constant_values: dict[tuple[str, str], int] = field(default_factory=dict)


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def identifier(name: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", name).upper()
    if not value or value[0].isdigit():
        value = "_" + value
    return value


def check_names(groups: dict[str, list[str]]) -> None:
    used: dict[str, str] = {}
    for kind, names in groups.items():
        for name in names:
            value = f"{kind}_{identifier(name)}"
            if value in used:
                fail(f"asset identifier collision: {used[value]!r} and {kind} {name!r} both become {value}")
            used[value] = f"{kind} {name}"


def parse_key(value: Any) -> int:
    if isinstance(value, int) and 0 <= value <= 0xFFFF:
        return value
    if isinstance(value, list) and len(value) == 3 and all(isinstance(item, int) and 0 <= item <= 255 for item in value):
        return rgb565(*value)
    fail("color_key must be an RGB565 integer or [red, green, blue]")


def convert_pixels(rgba: list[tuple[int, int, int, int]], explicit_key: Any = None) -> tuple[list[int], int | None]:
    opaque: set[int] = set()
    transparent = False
    for red, green, blue, alpha in rgba:
        if alpha not in (0, 255):
            fail("sprite images require alpha values of exactly 0 or 255")
        if alpha == 0:
            transparent = True
        else:
            opaque.add(rgb565(red, green, blue))
    if not transparent:
        return [rgb565(red, green, blue) for red, green, blue, _ in rgba], None
    key = parse_key(explicit_key) if explicit_key is not None else next((value for value in range(0x10000) if value not in opaque), None)
    if key is None:
        fail("no unused RGB565 color remains for transparency")
    if key in opaque:
        fail(f"color key 0x{key:04x} collides with an opaque converted pixel")
    return [key if alpha == 0 else rgb565(red, green, blue) for red, green, blue, alpha in rgba], key


def open_frames(path: Path) -> tuple[list[tuple[int, int, list[tuple[int, int, int, int]], int]], int]:
    try:
        from PIL import Image as PillowImage
    except ImportError:
        fail("asset conversion requires Pillow; install it with 'python3 -m pip install Pillow'")
    try:
        source = PillowImage.open(path)
    except OSError as error:
        fail(f"cannot decode image {path}: {error}")
    frames = []
    count = getattr(source, "n_frames", 1)
    for index in range(count):
        source.seek(index)
        frame = source.convert("RGBA")
        duration = int(source.info.get("duration", 0))
        frames.append((frame.width, frame.height, list(frame.getdata()), duration))
    if count == 1:
        repeat = 1
    elif "loop" not in source.info:
        repeat = 1
    else:
        loop = int(source.info["loop"])
        repeat = 0 if loop == 0 else loop + 1
    return frames, repeat


def add_image_entry(assets: AssetSet, base: Path, entry: dict[str, Any], flags: dict[str, int]) -> None:
    name = require_string(entry, "name")
    source = base / require_string(entry, "source")
    frames, gif_repeat = open_frames(source)
    resize = entry.get("resize")
    transparent_rgb = entry.get("transparent_rgb")
    if resize is not None:
        if not isinstance(resize, list) or len(resize) != 2:
            fail("resize must be [width, height]")
        resized_width = integer(resize[0], "resize width")
        resized_height = integer(resize[1], "resize height")
        if resized_width <= 0 or resized_height <= 0:
            fail("resize dimensions must be positive")
        resized_frames = []
        for width, height, rgba, duration in frames:
            pixels = [rgba[(y * height // resized_height) * width + (x * width // resized_width)]
                      for y in range(resized_height) for x in range(resized_width)]
            resized_frames.append((resized_width, resized_height, pixels, duration))
        frames = resized_frames
    if transparent_rgb is not None:
        if not isinstance(transparent_rgb, list) or len(transparent_rgb) != 3:
            fail("transparent_rgb must be [red, green, blue]")
        color = tuple(integer(value, "transparent RGB component") for value in transparent_rgb)
        if any(value < 0 or value > 255 for value in color):
            fail("transparent RGB components must be between 0 and 255")
        tolerance = integer(entry.get("transparent_tolerance", 0), "transparent tolerance")
        if tolerance < 0 or tolerance > 255:
            fail("transparent tolerance must be between 0 and 255")
        keyed_frames = []
        for width, height, rgba, duration in frames:
            pixels = [(red, green, blue, 0 if max(abs(red - color[0]), abs(green - color[1]),
                                                   abs(blue - color[2])) <= tolerance else alpha)
                      for red, green, blue, alpha in rgba]
            keyed_frames.append((width, height, pixels, duration))
        frames = keyed_frames
    duration_overrides = entry.get("durations_ms")
    generated_sprite_ids: list[int] = []
    for frame_index, (width, height, rgba, duration) in enumerate(frames):
        pixels, key = convert_pixels(rgba, entry.get("color_key"))
        image_name = name if len(frames) == 1 else f"{name}_frame_{frame_index}"
        image_id = len(assets.images)
        assets.images.append(Image(image_name, width, height, pixels, key))
        regions = entry.get("sprites")
        if regions is None:
            regions = [{"name": image_name, "x": 0, "y": 0, "width": width, "height": height}]
        if len(frames) > 1 and len(regions) != 1:
            fail(f"animated GIF {source} may define only one full-frame sprite")
        for region in regions:
            sprite_name = require_string(region, "name")
            if len(frames) > 1:
                sprite_name = f"{name}_frame_{frame_index}"
            x = integer(region.get("x", 0), "sprite x")
            y = integer(region.get("y", 0), "sprite y")
            region_width = integer(region.get("width", width), "sprite width")
            region_height = integer(region.get("height", height), "sprite height")
            if x < 0 or y < 0 or region_width <= 0 or region_height <= 0 or x + region_width > width or y + region_height > height:
                fail(f"sprite {sprite_name!r} lies outside image {image_name!r}")
            pivot = region.get("pivot", [0, 0])
            if not isinstance(pivot, list) or len(pivot) != 2:
                fail(f"sprite {sprite_name!r} pivot must be [x, y]")
            flag_value = flags_value(region.get("flags", 0), flags)
            generated_sprite_ids.append(len(assets.sprites))
            assets.sprites.append(Sprite(sprite_name, image_id, x, y, region_width, region_height,
                                          integer(pivot[0], "pivot x"), integer(pivot[1], "pivot y"), flag_value))
    if len(frames) > 1:
        durations = []
        for index, frame in enumerate(frames):
            duration = frame[3]
            if duration_overrides is not None:
                if not isinstance(duration_overrides, list) or len(duration_overrides) != len(frames):
                    fail(f"{name} durations_ms must contain one value per GIF frame")
                duration = integer(duration_overrides[index], "GIF frame duration")
            elif duration < 10:
                duration = 10
            if duration <= 0:
                fail("GIF frame duration must be positive")
            durations.append((generated_sprite_ids[index], duration))
        assets.animations.append(Animation(name, durations,
                                            unsigned_32(entry.get("repeat_count", gif_repeat), "repeat count")))


def integer(value: Any, label: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        fail(f"{label} must be an integer")
    return value


def unsigned_32(value: Any, label: str) -> int:
    result = integer(value, label)
    if result < 0 or result > 0xFFFFFFFF:
        fail(f"{label} must fit an unsigned 32-bit integer")
    return result


def require_string(value: dict[str, Any], key: str) -> str:
    result = value.get(key)
    if not isinstance(result, str) or not result:
        fail(f"{key} must be a non-empty string")
    return result


def flags_value(value: Any, flags: dict[str, int]) -> int:
    if isinstance(value, int) and not isinstance(value, bool):
        return unsigned_32(value, "flags")
    if isinstance(value, list) and all(isinstance(item, str) for item in value):
        result = 0
        for name in value:
            if name not in flags:
                fail(f"unknown flag {name!r}")
            result |= flags[name]
        return result
    fail("flags must be an integer or list of flag names")


def load_manifest(path: Path) -> AssetSet:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read asset manifest {path}: {error}")
    if not isinstance(manifest, dict) or manifest.get("version") != 1:
        fail("asset manifest version must be 1")
    assets = AssetSet(require_string(manifest, "name"))
    raw_flags = manifest.get("flags", {})
    if not isinstance(raw_flags, dict):
        fail("flags must be an object")
    flags = {name: unsigned_32(value, f"flag {name}") for name, value in raw_flags.items()}
    for entry in manifest.get("images", []):
        if not isinstance(entry, dict):
            fail("each images entry must be an object")
        add_image_entry(assets, path.parent, entry, flags)
    sprite_ids = {sprite.name: index for index, sprite in enumerate(assets.sprites)}
    for entry in manifest.get("animations", []):
        frames = []
        for frame in entry.get("frames", []):
            sprite_name = require_string(frame, "sprite")
            if sprite_name not in sprite_ids:
                fail(f"unknown animation sprite {sprite_name!r}")
            duration = integer(frame.get("duration_ms"), "animation duration_ms")
            if duration <= 0:
                fail("animation duration_ms must be positive")
            frames.append((sprite_ids[sprite_name], duration))
        if not frames:
            fail("animation needs at least one frame")
        assets.animations.append(Animation(require_string(entry, "name"), frames,
                                            unsigned_32(entry.get("repeat_count", 0), "repeat count")))
    sprite_ids = {sprite.name: index for index, sprite in enumerate(assets.sprites)}
    for entry in manifest.get("metasprites", []):
        parts = []
        for part in entry.get("parts", []):
            sprite_name = require_string(part, "sprite")
            if sprite_name not in sprite_ids:
                fail(f"unknown metasprite sprite {sprite_name!r}")
            rotation = integer(part.get("rotation", 0), "part rotation")
            opacity = integer(part.get("opacity", 255), "part opacity")
            if rotation < 0 or rotation > 3 or opacity < 0 or opacity > 255:
                fail("metasprite rotation must be 0-3 and opacity must be 0-255")
            parts.append({"sprite": sprite_ids[sprite_name], "x": integer(part.get("x", 0), "part x"),
                          "y": integer(part.get("y", 0), "part y"), "rotation": rotation,
                          "mirror_x": bool(part.get("mirror_x", False)), "mirror_y": bool(part.get("mirror_y", False)),
                          "opacity": opacity})
        if not parts:
            fail("metasprite needs at least one part")
        assets.metasprites.append(Metasprite(require_string(entry, "name"), parts))
    for entry in manifest.get("maps", []):
        assets.maps.append(load_tiled_map(assets, path.parent / require_string(entry, "source"),
                                          require_string(entry, "name"), flags))
    assets.constants = {"FLAG": list(flags), "SPRITE": [item.name for item in assets.sprites],
                        "ANIMATION": [item.name for item in assets.animations],
                        "METASPRITE": [item.name for item in assets.metasprites],
                        "MAP": [item["name"] for item in assets.maps]}
    for flag_name, flag_value in flags.items():
        assets.constant_values[("FLAG", flag_name)] = flag_value
    for kind in ("SPRITE", "ANIMATION", "METASPRITE", "MAP"):
        for index, item_name in enumerate(assets.constants[kind]):
            assets.constant_values[(kind, item_name)] = index
    for tiled_map in assets.maps:
        for index, layer in enumerate(tiled_map["layers"]):
            constant_name = f"{tiled_map['name']}_{layer['name']}"
            assets.constants.setdefault("LAYER", []).append(constant_name)
            assets.constant_values[("LAYER", constant_name)] = index
        for obj in tiled_map["objects"]:
            if obj["name"]:
                constant_name = f"{tiled_map['name']}_{obj['name']}"
                assets.constants.setdefault("OBJECT", []).append(constant_name)
                assets.constant_values[("OBJECT", constant_name)] = obj["id"]
    check_names(assets.constants)
    return assets


def load_tiled_map(assets: AssetSet, path: Path, name: str, flags: dict[str, int]) -> dict[str, Any]:
    try:
        tiled = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read Tiled map {path}: {error}")
    if tiled.get("infinite", False) or tiled.get("orientation") != "orthogonal":
        fail(f"Tiled map {path} must be finite and orthogonal")
    width = integer(tiled.get("width"), "map width")
    height = integer(tiled.get("height"), "map height")
    tile_width = integer(tiled.get("tilewidth"), "tile width")
    tile_height = integer(tiled.get("tileheight"), "tile height")
    gid_map: dict[int, int] = {}
    for tileset_ref in tiled.get("tilesets", []):
        if "source" in tileset_ref:
            external_path = path.parent / tileset_ref["source"]
            try:
                tileset = json.loads(external_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError) as error:
                fail(f"cannot read Tiled tileset {external_path}: {error}")
            tileset_base = external_path.parent
        else:
            tileset = tileset_ref
            tileset_base = path.parent
        first_gid = integer(tileset_ref.get("firstgid"), "tileset firstgid")
        if "image" not in tileset:
            fail("collection-of-images Tiled tilesets are not supported")
        image_path = tileset_base / require_string(tileset, "image")
        frames, _ = open_frames(image_path)
        if len(frames) != 1:
            fail("Tiled tileset image must not be animated")
        image_width, image_height, rgba, _ = frames[0]
        pixels, key = convert_pixels(rgba)
        image_id = len(assets.images)
        tileset_name = str(tileset.get("name", image_path.stem))
        assets.images.append(Image(f"{name}_{tileset_name}", image_width, image_height, pixels, key))
        tw = integer(tileset.get("tilewidth", tile_width), "tileset tilewidth")
        th = integer(tileset.get("tileheight", tile_height), "tileset tileheight")
        columns = integer(tileset.get("columns", image_width // tw), "tileset columns")
        tile_count = integer(tileset.get("tilecount", columns * (image_height // th)), "tileset tilecount")
        margin = integer(tileset.get("margin", 0), "tileset margin")
        spacing = integer(tileset.get("spacing", 0), "tileset spacing")
        metadata = {integer(item.get("id"), "tile id"): item for item in tileset.get("tiles", [])}
        local_sprite_ids = []
        for local_id in range(tile_count):
            x = margin + (local_id % columns) * (tw + spacing)
            y = margin + (local_id // columns) * (th + spacing)
            if x + tw > image_width or y + th > image_height:
                fail(f"tileset tile {local_id} lies outside image")
            properties = metadata.get(local_id, {}).get("properties", [])
            tile_flags = 0
            for prop in properties:
                if prop.get("type") not in ("int", "bool"):
                    fail("only integer and boolean tile properties are supported")
                prop_name = require_string(prop, "name")
                if prop_name in flags and bool(prop.get("value")):
                    tile_flags |= flags[prop_name]
            sprite_id = len(assets.sprites)
            assets.sprites.append(Sprite(f"{name}_{tileset_name}_{local_id}", image_id, x, y, tw, th, 0, 0, tile_flags))
            gid_map[first_gid + local_id] = sprite_id
            local_sprite_ids.append(sprite_id)
        for local_id, item in metadata.items():
            if "animation" in item:
                frames_out = []
                for frame in item["animation"]:
                    frame_id = integer(frame.get("tileid"), "animation tileid")
                    if frame_id < 0 or frame_id >= len(local_sprite_ids):
                        fail("Tiled animation references invalid tile")
                    duration = integer(frame.get("duration"), "animation duration")
                    if duration <= 0:
                        fail("Tiled animation duration must be positive")
                    frames_out.append((local_sprite_ids[frame_id], max(duration, 10)))
                if not frames_out:
                    fail("Tiled animation needs at least one frame")
                assets.animations.append(Animation(f"{name}_{tileset_name}_{local_id}", frames_out, 0,
                                                    local_sprite_ids[local_id]))
    layers = []
    objects = []
    properties = []
    for layer in tiled.get("layers", []):
        layer_name = require_string(layer, "name")
        if layer.get("type") == "tilelayer":
            data = layer.get("data")
            if not isinstance(data, list) or len(data) != width * height:
                fail(f"tile layer {layer_name!r} must contain width * height cells")
            cells = []
            for raw_gid in data:
                gid = integer(raw_gid, "tile gid")
                if gid & GID_RESERVED:
                    fail(f"tile layer {layer_name!r} contains malformed reserved GID bit")
                transforms = gid & (GID_HORIZONTAL | GID_VERTICAL | GID_DIAGONAL)
                plain_gid = gid & GID_MASK
                if plain_gid == 0:
                    cells.append(0)
                elif plain_gid not in gid_map:
                    fail(f"tile layer {layer_name!r} references unknown GID {plain_gid}")
                else:
                    cells.append(transforms | (gid_map[plain_gid] + 1))
            layers.append({"name": layer_name, "type": 0, "cells": cells, "first": 0, "count": len(cells)})
        elif layer.get("type") == "objectgroup":
            first = len(objects)
            for obj in layer.get("objects", []):
                unsupported = any(bool(obj.get(key)) for key in ("ellipse", "text", "polygon", "polyline", "template"))
                if unsupported or obj.get("rotation", 0) != 0:
                    fail(f"object {obj.get('name', obj.get('id'))!r} uses unsupported geometry or rotation")
                geometry = [obj.get(key, 0) for key in ("x", "y", "width", "height")]
                if any(not isinstance(value, int) or isinstance(value, bool) for value in geometry):
                    fail("object geometry must use integral coordinates")
                first_property = len(properties)
                for prop in obj.get("properties", []):
                    if prop.get("type") != "int" or not isinstance(prop.get("value"), int):
                        fail("object properties must be integers")
                    properties.append((require_string(prop, "name"), prop["value"]))
                tile = 0
                shape = 0 if obj.get("point", False) else 1
                if "gid" in obj:
                    raw_gid = integer(obj["gid"], "object gid")
                    plain_gid = raw_gid & GID_MASK
                    if plain_gid not in gid_map or raw_gid & GID_RESERVED:
                        fail("tile object contains malformed or unknown GID")
                    tile = (raw_gid & (GID_HORIZONTAL | GID_VERTICAL | GID_DIAGONAL)) | (gid_map[plain_gid] + 1)
                    shape = 2
                objects.append({"id": integer(obj.get("id"), "object id"), "name": str(obj.get("name", "")),
                                "class": str(obj.get("class", obj.get("type", ""))), "shape": shape,
                                "x": geometry[0], "y": geometry[1], "width": geometry[2], "height": geometry[3],
                                "tile": tile, "first_property": first_property,
                                "property_count": len(properties) - first_property})
            layers.append({"name": layer_name, "type": 1, "first": first, "count": len(objects) - first})
        else:
            fail(f"unsupported Tiled layer type {layer.get('type')!r}")
    return {"name": name, "width": width, "height": height, "tile_width": tile_width, "tile_height": tile_height,
            "layers": layers, "objects": objects, "properties": properties}


def align(data: bytearray) -> None:
    while len(data) & 3:
        data.append(0)


def write_tsp(assets: AssetSet, path: Path) -> None:
    frame_count = sum(len(item.frames) for item in assets.animations)
    part_count = sum(len(item.parts) for item in assets.metasprites)
    data = bytearray(60)
    offsets = []
    for count, stride in ((len(assets.images), 20), (len(assets.sprites), 32), (len(assets.animations), 16),
                          (frame_count, 8), (len(assets.metasprites), 8), (part_count, 24)):
        align(data)
        offsets.append(len(data))
        data.extend(bytes(count * stride))
    pixel_offsets = []
    for image in assets.images:
        align(data)
        pixel_offsets.append(len(data))
        data.extend(struct.pack(f"<{len(image.pixels)}H", *image.pixels))
    for index, image in enumerate(assets.images):
        struct.pack_into("<5I", data, offsets[0] + index * 20, image.width, image.height, pixel_offsets[index],
                         image.key is not None, image.key or 0)
    for index, sprite in enumerate(assets.sprites):
        struct.pack_into("<IiiIIiiI", data, offsets[1] + index * 32, sprite.image, sprite.x, sprite.y, sprite.width,
                         sprite.height, sprite.pivot_x, sprite.pivot_y, sprite.flags)
    frame_index = 0
    for index, animation in enumerate(assets.animations):
        trigger = animation.frames[0][0] if animation.trigger is None else animation.trigger
        struct.pack_into("<4I", data, offsets[2] + index * 16, frame_index, len(animation.frames), animation.repeat,
                         trigger)
        for sprite, duration in animation.frames:
            struct.pack_into("<2I", data, offsets[3] + frame_index * 8, sprite, duration)
            frame_index += 1
    part_index = 0
    for index, metasprite in enumerate(assets.metasprites):
        struct.pack_into("<2I", data, offsets[4] + index * 8, part_index, len(metasprite.parts))
        for part in metasprite.parts:
            transform = int(part["mirror_x"]) | (int(part["mirror_y"]) << 1)
            struct.pack_into("<IiiI2B6x", data, offsets[5] + part_index * 24, part["sprite"], part["x"], part["y"],
                             part["rotation"], transform, part["opacity"])
            part_index += 1
    struct.pack_into("<4s14I", data, 0, b"TSP1", 1, len(data), len(assets.images), len(assets.sprites),
                     len(assets.animations), frame_count, len(assets.metasprites), part_count, *offsets)
    path.write_bytes(data)


def string_table(values: list[str]) -> tuple[bytes, dict[str, int]]:
    data = bytearray(b"\0")
    offsets = {"": 0}
    for value in values:
        if value not in offsets:
            offsets[value] = len(data)
            data.extend(value.encode("utf-8") + b"\0")
    return bytes(data), offsets


def write_tmap(tiled: dict[str, Any], path: Path) -> None:
    strings, names = string_table([layer["name"] for layer in tiled["layers"]] +
                                  [value for obj in tiled["objects"] for value in (obj["name"], obj["class"])] +
                                  [name for name, _ in tiled["properties"]])
    data = bytearray(64)
    align(data); layers_offset = len(data); data.extend(bytes(len(tiled["layers"]) * 20))
    align(data); cells_offset = len(data)
    cell_index = 0
    for layer in tiled["layers"]:
        if layer["type"] == 0:
            layer["first"] = cell_index
            data.extend(struct.pack(f"<{len(layer['cells'])}I", *layer["cells"]))
            cell_index += len(layer["cells"])
    align(data); objects_offset = len(data); data.extend(bytes(len(tiled["objects"]) * 48))
    align(data); properties_offset = len(data); data.extend(bytes(len(tiled["properties"]) * 12))
    align(data); strings_offset = len(data); data.extend(strings)
    for index, layer in enumerate(tiled["layers"]):
        struct.pack_into("<5I", data, layers_offset + index * 20, names[layer["name"]], layer["type"],
                         layer["first"], layer["count"], 0)
    for index, obj in enumerate(tiled["objects"]):
        struct.pack_into("<4I2i6I", data, objects_offset + index * 48, obj["id"], names[obj["name"]],
                         names[obj["class"]], obj["shape"], obj["x"], obj["y"], obj["width"], obj["height"],
                         obj["tile"], 0, obj["first_property"], obj["property_count"])
    for index, (name, value) in enumerate(tiled["properties"]):
        struct.pack_into("<IiI", data, properties_offset + index * 12, names[name], value, 0)
    struct.pack_into("<4s15I", data, 0, b"TMP1", 1, len(data), tiled["width"], tiled["height"],
                     tiled["tile_width"], tiled["tile_height"], len(tiled["layers"]), len(tiled["objects"]),
                     len(tiled["properties"]), len(strings), layers_offset, cells_offset, objects_offset,
                     properties_offset, strings_offset)
    path.write_bytes(data)


def write_c(assets: AssetSet, output: Path) -> None:
    prefix = identifier(assets.name).lower()
    header = ["#ifndef TABOS_ASSETS_" + identifier(assets.name) + "_H", "#define TABOS_ASSETS_" + identifier(assets.name) + "_H", "", "#include <tabos/tilemap.h>", ""]
    for kind, names in assets.constants.items():
        for name in names:
            value = assets.constant_values[(kind, name)]
            header.append(f"#define {identifier(assets.name)}_{kind}_{identifier(name)} {value}U")
    header.extend(["", f"extern const tabos_sprite_set_t {prefix}_sprites;"])
    for tiled in assets.maps:
        header.append(f"extern tabos_tilemap_t {prefix}_{identifier(tiled['name']).lower()};")
    header.extend(["", "#endif", ""])
    output.with_suffix(".h").write_text("\n".join(header), encoding="utf-8")
    source = [f'#include "{output.with_suffix(".h").name}"', "", "#include <stddef.h>", ""]
    for index, image in enumerate(assets.images):
        values = ", ".join(f"0x{pixel:04x}U" for pixel in image.pixels)
        source.append(f"static const tabos_color_t image_{index}[] = {{{values}}};")
    source.append("")
    source.append("static const tabos_sprite_image_t images[] = {")
    for index, image in enumerate(assets.images):
        source.append(f"    {{.pixels = image_{index}, .width = {image.width}U, .height = {image.height}U, .color_key = 0x{(image.key or 0):04x}U, .color_key_enabled = {'true' if image.key is not None else 'false'}}},")
    if not assets.images:
        source.append("    {0},")
    source.extend(["};", "", "static const tabos_sprite_t sprites[] = {"])
    for sprite in assets.sprites:
        source.append(f"    {{.image = {sprite.image}U, .x = {sprite.x}, .y = {sprite.y}, .width = {sprite.width}U, .height = {sprite.height}U, .pivot_x = {sprite.pivot_x}, .pivot_y = {sprite.pivot_y}, .flags = {sprite.flags}U}},")
    if not assets.sprites:
        source.append("    {0},")
    source.extend(["};", ""])
    for index, animation in enumerate(assets.animations):
        frames = ", ".join(f"{{.sprite = {sprite}U, .duration_ms = {duration}U}}" for sprite, duration in animation.frames)
        source.append(f"static const tabos_sprite_frame_t frames_{index}[] = {{{frames}}};")
    source.append("static const tabos_sprite_animation_t animations[] = {")
    for index, animation in enumerate(assets.animations):
        trigger = animation.frames[0][0] if animation.trigger is None else animation.trigger
        source.append(f"    {{.frames = frames_{index}, .frame_count = {len(animation.frames)}U, .repeat_count = {animation.repeat}U, .trigger_sprite = {trigger}U}},")
    if not assets.animations:
        source.append("    {0},")
    source.extend(["};", ""])
    for index, metasprite in enumerate(assets.metasprites):
        source.append(f"static const tabos_metasprite_part_t parts_{index}[] = {{")
        for part in metasprite.parts:
            source.append(f"    {{.sprite = {part['sprite']}U, .x = {part['x']}, .y = {part['y']}, .rotation = {part['rotation']}, .mirror_x = {'true' if part['mirror_x'] else 'false'}, .mirror_y = {'true' if part['mirror_y'] else 'false'}, .opacity = {part['opacity']}U}},")
        source.append("};")
    source.append("static const tabos_metasprite_t metasprites[] = {")
    for index, metasprite in enumerate(assets.metasprites):
        source.append(f"    {{.parts = parts_{index}, .part_count = {len(metasprite.parts)}U}},")
    if not assets.metasprites:
        source.append("    {0},")
    source.extend(["};", "", f"const tabos_sprite_set_t {prefix}_sprites = {{",
                   f"    .images = images, .image_count = {len(assets.images)}U,",
                   f"    .sprites = sprites, .sprite_count = {len(assets.sprites)}U,",
                   f"    .animations = animations, .animation_count = {len(assets.animations)}U,",
                   f"    .metasprites = metasprites, .metasprite_count = {len(assets.metasprites)}U,", "};", ""])
    for map_index, tiled in enumerate(assets.maps):
        for layer_index, layer in enumerate(tiled["layers"]):
            if layer["type"] == 0:
                values = ", ".join(f"0x{cell:08x}U" for cell in layer["cells"])
                source.append(f"static tabos_tile_t map_{map_index}_cells_{layer_index}[] = {{{values}}};")
        for object_index, obj in enumerate(tiled["objects"]):
            props = tiled["properties"][obj["first_property"]:obj["first_property"] + obj["property_count"]]
            if props:
                values = ", ".join(f'{{.name = {json.dumps(name)}, .value = {value}}}' for name, value in props)
                source.append(f"static const tabos_tilemap_property_t map_{map_index}_properties_{object_index}[] = {{{values}}};")
        source.append(f"static const tabos_tilemap_object_t map_{map_index}_objects[] = {{")
        for object_index, obj in enumerate(tiled["objects"]):
            prop_pointer = f"map_{map_index}_properties_{object_index}" if obj["property_count"] else "NULL"
            source.append(f'    {{.id = {obj["id"]}U, .name = {json.dumps(obj["name"])}, .type = {json.dumps(obj["class"])}, .shape = {obj["shape"]}, .x = {obj["x"]}, .y = {obj["y"]}, .width = {obj["width"]}U, .height = {obj["height"]}U, .tile = 0x{obj["tile"]:08x}U, .properties = {prop_pointer}, .property_count = {obj["property_count"]}U}},')
        if not tiled["objects"]:
            source.append("    {0},")
        source.extend(["};", f"static tabos_tilemap_layer_t map_{map_index}_layers[] = {{"])
        object_index = 0
        for layer_index, layer in enumerate(tiled["layers"]):
            if layer["type"] == 0:
                source.append(f'    {{.name = {json.dumps(layer["name"])}, .type = TABOS_TILEMAP_LAYER_TILES, .cells = map_{map_index}_cells_{layer_index}}},')
            else:
                source.append(f'    {{.name = {json.dumps(layer["name"])}, .type = TABOS_TILEMAP_LAYER_OBJECTS, .objects = map_{map_index}_objects + {layer["first"]}U, .object_count = {layer["count"]}U}},')
        if not tiled["layers"]:
            source.append("    {0},")
        map_symbol = f"{prefix}_{identifier(tiled['name']).lower()}"
        source.extend(["};", f"tabos_tilemap_t {map_symbol} = {{.width = {tiled['width']}U, .height = {tiled['height']}U, .tile_width = {tiled['tile_width']}U, .tile_height = {tiled['tile_height']}U, .layers = map_{map_index}_layers, .layer_count = {len(tiled['layers'])}U}};", ""])
    output.with_suffix(".c").write_text("\n".join(source), encoding="utf-8")


def command_assets_build(args: argparse.Namespace) -> None:
    manifest = Path(args.manifest).resolve()
    assets = load_manifest(manifest)
    output = Path(args.output).resolve() if args.output else manifest.parent / "build"
    output.mkdir(parents=True, exist_ok=True)
    stem = output / assets.name
    write_tsp(assets, stem.with_suffix(".tsp"))
    for tiled in assets.maps:
        write_tmap(tiled, output / f"{tiled['name']}.tmap")
    write_c(assets, stem)
    print(f"wrote {stem.with_suffix('.tsp')}")
    for tiled in assets.maps:
        print(f"wrote {output / (tiled['name'] + '.tmap')}")
    print(f"wrote {stem.with_suffix('.c')} and {stem.with_suffix('.h')}")
