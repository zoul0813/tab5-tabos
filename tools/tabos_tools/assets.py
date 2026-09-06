from __future__ import annotations

import argparse
import json
import math
import re
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .common import fail, warn


GID_HORIZONTAL = 0x80000000
GID_VERTICAL = 0x40000000
GID_DIAGONAL = 0x20000000
GID_RESERVED = 0x10000000
GID_MASK = 0x0FFFFFFF
TILED_OBJECT_ALIGNMENTS = {
    "topleft": (0, 0),
    "top": (1, 0),
    "topright": (2, 0),
    "left": (0, 1),
    "center": (1, 1),
    "right": (2, 1),
    "bottomleft": (0, 2),
    "bottom": (1, 2),
    "bottomright": (2, 2),
}


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
    if isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= 0xFFFF:
        return value
    if (isinstance(value, list) and len(value) == 3 and
            all(isinstance(item, int) and not isinstance(item, bool) and 0 <= item <= 255 for item in value)):
        return rgb565(*value)
    fail("color_key must be an RGB565 integer or [red, green, blue]")


def require_binary_alpha(rgba: list[tuple[int, int, int, int]]) -> None:
    if any(alpha not in (0, 255) for _, _, _, alpha in rgba):
        fail("sprite images require alpha values of exactly 0 or 255")


def convert_pixels(rgba: list[tuple[int, int, int, int]], explicit_key: Any = None) -> tuple[list[int], int | None]:
    require_binary_alpha(rgba)
    opaque: set[int] = set()
    transparent = False
    for red, green, blue, alpha in rgba:
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


def open_frames(path: Path, accepted_formats: set[str]) -> tuple[list[tuple[int, int, list[tuple[int, int, int, int]], int]], int]:
    try:
        from PIL import Image as PillowImage
    except ImportError:
        fail("asset conversion requires Pillow; install it with 'python3 -m pip install Pillow'")
    try:
        source = PillowImage.open(path)
    except OSError as error:
        fail(f"cannot decode image {path}: {error}")
    if source.format not in accepted_formats:
        expected = "/".join(sorted(accepted_formats))
        fail(f"image {path} uses {source.format or 'unknown'} format; expected {expected}")
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
    frames, gif_repeat = open_frames(source, {"GIF", "PNG"})
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
        for _, _, rgba, _ in frames:
            require_binary_alpha(rgba)
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
        regions = require_list(regions, f"image {source} sprites")
        if len(frames) > 1 and len(regions) != 1:
            fail(f"animated GIF {source} may define only one full-frame sprite")
        for region_index, region_value in enumerate(regions):
            region = require_object(region_value, f"image {source} sprites[{region_index}]")
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
                                          signed_32(pivot[0], "pivot x"), signed_32(pivot[1], "pivot y"), flag_value))
    if len(frames) > 1:
        durations = []
        for index, frame in enumerate(frames):
            duration = frame[3]
            if duration_overrides is not None:
                if not isinstance(duration_overrides, list) or len(duration_overrides) != len(frames):
                    fail(f"{name} durations_ms must contain one value per GIF frame")
                duration = unsigned_32(duration_overrides[index], "GIF frame duration")
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


def signed_32(value: Any, label: str) -> int:
    result = integer(value, label)
    if result < -0x80000000 or result > 0x7FFFFFFF:
        fail(f"{label} must fit a signed 32-bit integer")
    return result


def finite_number(value: Any, label: str) -> int | float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        fail(f"{label} must be a finite number")
    if isinstance(value, float) and not math.isfinite(value):
        fail(f"{label} must be a finite number")
    return value


def rounded_integer(value: Any, label: str, minimum: int, maximum: int) -> int:
    value = finite_number(value, label)
    if value < minimum or value > maximum:
        fail(f"{label} must be between {minimum} and {maximum}")
    if isinstance(value, int):
        result = value
    else:
        result = math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)
    if result != value:
        warn(f"{label} {value!r} rounded to {result}")
    return result


def tiled_object_origin(x: int | float, y: int | float, width: int | float, height: int | float,
                        alignment: str) -> tuple[int | float, int | float]:
    horizontal, vertical = TILED_OBJECT_ALIGNMENTS[alignment]
    return x - width * horizontal / 2, y - height * vertical / 2


def require_list(value: Any, label: str) -> list[Any]:
    if not isinstance(value, list):
        fail(f"{label} must be an array")
    return value


def require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        fail(f"{label} must be an object")
    return value


def boolean(value: Any, label: str) -> bool:
    if not isinstance(value, bool):
        fail(f"{label} must be a boolean")
    return value


def require_string(value: dict[str, Any], key: str, label: str | None = None) -> str:
    result = value.get(key)
    if not isinstance(result, str) or not result:
        fail(f"{label or key} must be a non-empty string")
    return result


def optional_string(value: dict[str, Any], key: str, label: str) -> str:
    result = value.get(key, "")
    if not isinstance(result, str):
        fail(f"{label} must be a string")
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
    manifest = require_object(manifest, f"asset manifest {path}")
    if integer(manifest.get("version"), f"asset manifest {path} version") != 1:
        fail(f"asset manifest {path} version must be 1")
    assets = AssetSet(require_string(manifest, "name", f"asset manifest {path} name"))
    raw_flags = manifest.get("flags", {})
    if not isinstance(raw_flags, dict):
        fail("flags must be an object")
    flags: dict[str, int] = {}
    used_flag_values: dict[int, str] = {}
    for name, value in raw_flags.items():
        if not name:
            fail(f"asset manifest {path} flag names must be non-empty")
        flag = unsigned_32(value, f"flag {name}")
        if flag == 0 or (flag & (flag - 1)) != 0:
            fail(f"flag {name!r} must be one nonzero 32-bit bit")
        if flag in used_flag_values:
            fail(f"flags {used_flag_values[flag]!r} and {name!r} use the same bit")
        flags[name] = flag
        used_flag_values[flag] = name
    manifest_image_sources: dict[Path, int] = {}
    for index, entry_value in enumerate(require_list(manifest.get("images", []), f"asset manifest {path} images")):
        entry = require_object(entry_value, f"asset manifest {path} images[{index}]")
        image_source = (path.parent / require_string(entry, "source")).resolve()
        if image_source in manifest_image_sources:
            original_index = manifest_image_sources[image_source]
            fail(f"asset manifest {path} images[{index}] duplicates source from images[{original_index}]: "
                 f"{image_source}")
        manifest_image_sources[image_source] = index
        add_image_entry(assets, path.parent, entry, flags)
    for index, entry_value in enumerate(require_list(manifest.get("maps", []), f"asset manifest {path} maps")):
        entry = require_object(entry_value, f"asset manifest {path} maps[{index}]")
        assets.maps.append(load_tiled_map(assets, path.parent / require_string(entry, "source"),
                                          require_string(entry, "name"), flags, set(manifest_image_sources)))
    sprite_ids = {sprite.name: index for index, sprite in enumerate(assets.sprites)}
    for index, entry_value in enumerate(require_list(manifest.get("animations", []),
                                                     f"asset manifest {path} animations")):
        entry = require_object(entry_value, f"asset manifest {path} animations[{index}]")
        frames = []
        for frame_index, frame_value in enumerate(require_list(entry.get("frames", []),
                                                               f"animation {index} frames")):
            frame = require_object(frame_value, f"animation {index} frames[{frame_index}]")
            sprite_name = require_string(frame, "sprite")
            if sprite_name not in sprite_ids:
                fail(f"unknown animation sprite {sprite_name!r}")
            duration = unsigned_32(frame.get("duration_ms"), "animation duration_ms")
            if duration <= 0:
                fail("animation duration_ms must be positive")
            frames.append((sprite_ids[sprite_name], duration))
        if not frames:
            fail("animation needs at least one frame")
        assets.animations.append(Animation(require_string(entry, "name"), frames,
                                            unsigned_32(entry.get("repeat_count", 0), "repeat count")))
    sprite_ids = {sprite.name: index for index, sprite in enumerate(assets.sprites)}
    for index, entry_value in enumerate(require_list(manifest.get("metasprites", []),
                                                     f"asset manifest {path} metasprites")):
        entry = require_object(entry_value, f"asset manifest {path} metasprites[{index}]")
        parts = []
        for part_index, part_value in enumerate(require_list(entry.get("parts", []),
                                                             f"metasprite {index} parts")):
            part = require_object(part_value, f"metasprite {index} parts[{part_index}]")
            sprite_name = require_string(part, "sprite")
            if sprite_name not in sprite_ids:
                fail(f"unknown metasprite sprite {sprite_name!r}")
            rotation = integer(part.get("rotation", 0), "part rotation")
            opacity = integer(part.get("opacity", 255), "part opacity")
            if rotation < 0 or rotation > 3 or opacity < 0 or opacity > 255:
                fail("metasprite rotation must be 0-3 and opacity must be 0-255")
            parts.append({"sprite": sprite_ids[sprite_name], "x": signed_32(part.get("x", 0), "part x"),
                          "y": signed_32(part.get("y", 0), "part y"), "rotation": rotation,
                          "mirror_x": boolean(part.get("mirror_x", False), "part mirror_x"),
                          "mirror_y": boolean(part.get("mirror_y", False), "part mirror_y"),
                          "opacity": opacity})
        if not parts:
            fail("metasprite needs at least one part")
        assets.metasprites.append(Metasprite(require_string(entry, "name"), parts))
    assets.constants = {"FLAG": list(flags), "SPRITE": [item.name for item in assets.sprites],
                        "ANIMATION": [item.name for item in assets.animations],
                        "METASPRITE": [item.name for item in assets.metasprites]}
    for flag_name, flag_value in flags.items():
        assets.constant_values[("FLAG", flag_name)] = flag_value
    for kind in ("SPRITE", "ANIMATION", "METASPRITE"):
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


def load_tiled_map(assets: AssetSet, path: Path, name: str, flags: dict[str, int],
                   manifest_image_sources: set[Path]) -> dict[str, Any]:
    try:
        tiled = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read Tiled map {path}: {error}")
    tiled = require_object(tiled, f"Tiled map {path}")
    if tiled.get("type") != "map":
        fail(f"Tiled map {path} type must be 'map'")
    if boolean(tiled.get("infinite", False), f"Tiled map {path} infinite") or tiled.get("orientation") != "orthogonal":
        fail(f"Tiled map {path} must be finite and orthogonal")
    width = signed_32(tiled.get("width"), f"Tiled map {path} width")
    height = signed_32(tiled.get("height"), f"Tiled map {path} height")
    tile_width = signed_32(tiled.get("tilewidth"), f"Tiled map {path} tilewidth")
    tile_height = signed_32(tiled.get("tileheight"), f"Tiled map {path} tileheight")
    if min(width, height, tile_width, tile_height) <= 0:
        fail(f"Tiled map {path} dimensions and tile dimensions must be positive")
    if width * height > 0xFFFFFFFF:
        fail(f"Tiled map {path} cell count must fit an unsigned 32-bit integer")
    gid_map: dict[int, int] = {}
    gid_alignments: dict[int, str] = {}
    tileset_values = require_list(tiled.get("tilesets", []), f"Tiled map {path} tilesets")
    for tileset_index, tileset_ref_value in enumerate(tileset_values):
        tileset_ref = require_object(tileset_ref_value, f"Tiled map {path} tilesets[{tileset_index}]")
        if "source" in tileset_ref:
            external_path = path.parent / require_string(
                tileset_ref, "source", f"Tiled map {path} tilesets[{tileset_index}] source")
            try:
                tileset = json.loads(external_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError) as error:
                fail(f"cannot read Tiled tileset {external_path}: {error}")
            tileset = require_object(tileset, f"Tiled tileset {external_path}")
            if tileset.get("type") != "tileset":
                fail(f"Tiled tileset {external_path} type must be 'tileset'")
            tileset_base = external_path.parent
            tileset_label = f"Tiled tileset {external_path}"
        else:
            tileset = tileset_ref
            tileset_base = path.parent
            tileset_label = f"inline tileset {tileset_index} in {path}"
            if "type" in tileset and tileset.get("type") != "tileset":
                fail(f"{tileset_label} type must be 'tileset'")
        first_gid = unsigned_32(tileset_ref.get("firstgid"), f"{tileset_label} firstgid")
        if first_gid == 0 or first_gid > GID_MASK:
            fail(f"{tileset_label} firstgid must be between 1 and {GID_MASK}")
        if "image" not in tileset:
            fail(f"{tileset_label}: collection-of-images tilesets are not supported")
        image_path = tileset_base / require_string(tileset, "image")
        if image_path.resolve() in manifest_image_sources:
            fail(f"{tileset_label} image {image_path} is already imported by the manifest; remove the images entry")
        frames, _ = open_frames(image_path, {"PNG"})
        if len(frames) != 1:
            fail(f"{tileset_label} image must not be animated")
        image_width, image_height, rgba, _ = frames[0]
        transparent_color = tileset.get("transparentcolor")
        property_values = require_list(tileset.get("properties", []), f"{tileset_label} properties")
        tileset_properties = [
            require_object(prop, f"{tileset_label} properties[{index}]")
            for index, prop in enumerate(property_values)
        ]
        if transparent_color is not None:
            if not isinstance(transparent_color, str) or re.fullmatch(r"#[0-9A-Fa-f]{6}", transparent_color) is None:
                fail(f"{tileset_label} transparentcolor must be #RRGGBB")
            color = tuple(int(transparent_color[index:index + 2], 16) for index in (1, 3, 5))
            tolerance = 0
            for prop in tileset_properties:
                if prop.get("name") == "transparent_tolerance":
                    if prop.get("type") != "int":
                        fail(f"{tileset_label} transparent_tolerance must be an integer property")
                    tolerance = integer(prop.get("value"), f"{tileset_label} transparent_tolerance")
                    if tolerance < 0 or tolerance > 255:
                        fail(f"{tileset_label} transparent_tolerance must be between 0 and 255")
            rgba = [(red, green, blue, 0 if max(abs(red - color[0]), abs(green - color[1]),
                                                abs(blue - color[2])) <= tolerance else alpha)
                    for red, green, blue, alpha in rgba]
        pixels, key = convert_pixels(rgba)
        image_id = len(assets.images)
        tileset_name = optional_string(tileset, "name", f"{tileset_label} name") or image_path.stem
        object_alignment = tileset.get("objectalignment", "unspecified")
        if object_alignment == "unspecified":
            object_alignment = "bottomleft"
        if not isinstance(object_alignment, str) or object_alignment not in TILED_OBJECT_ALIGNMENTS:
            fail(f"{tileset_label} objectalignment {object_alignment!r} is not supported")
        assets.images.append(Image(f"{name}_{tileset_name}", image_width, image_height, pixels, key))
        tw = signed_32(tileset.get("tilewidth", tile_width), f"{tileset_label} tilewidth")
        th = signed_32(tileset.get("tileheight", tile_height), f"{tileset_label} tileheight")
        if tw <= 0 or th <= 0:
            fail(f"{tileset_label} tile dimensions must be positive")
        columns = signed_32(tileset.get("columns", image_width // tw), f"{tileset_label} columns")
        tile_count = signed_32(tileset.get("tilecount", columns * (image_height // th)),
                               f"{tileset_label} tilecount")
        margin = signed_32(tileset.get("margin", 0), f"{tileset_label} margin")
        spacing = signed_32(tileset.get("spacing", 0), f"{tileset_label} spacing")
        if columns <= 0 or tile_count < 0 or margin < 0 or spacing < 0:
            fail(f"{tileset_label} columns must be positive; tilecount, margin, and spacing must be nonnegative")
        if tile_count and first_gid + tile_count - 1 > GID_MASK:
            fail(f"{tileset_label} GID range exceeds supported Tiled GIDs")
        metadata: dict[int, dict[str, Any]] = {}
        tile_values = require_list(tileset.get("tiles", []), f"{tileset_label} tiles")
        for tile_index, item_value in enumerate(tile_values):
            item = require_object(item_value, f"{tileset_label} tiles[{tile_index}]")
            local_id = unsigned_32(item.get("id"), f"{tileset_label} tile id")
            if local_id >= tile_count:
                fail(f"{tileset_label} tile id {local_id} is outside tilecount {tile_count}")
            if local_id in metadata:
                fail(f"{tileset_label} defines tile id {local_id} more than once")
            metadata[local_id] = item
        local_sprite_ids = []
        animation_names: dict[int, str] = {}
        animation_repeats: dict[int, int] = {}
        for local_id in range(tile_count):
            x = margin + (local_id % columns) * (tw + spacing)
            y = margin + (local_id // columns) * (th + spacing)
            if x + tw > image_width or y + th > image_height:
                fail(f"{tileset_label} tile {local_id} lies outside image")
            properties = require_list(metadata.get(local_id, {}).get("properties", []),
                                      f"{tileset_label} tile {local_id} properties")
            tile_flags = 0
            sprite_name = f"{name}_{tileset_name}_{local_id}"
            pivot_x = 0
            pivot_y = 0
            for property_index, prop_value in enumerate(properties):
                prop = require_object(prop_value,
                                      f"{tileset_label} tile {local_id} properties[{property_index}]")
                prop_name = require_string(prop, "name")
                prop_type = prop.get("type")
                if prop_name in ("name", "animation_name"):
                    if prop_type != "string" or not isinstance(prop.get("value"), str) or not prop["value"]:
                        fail(f"{prop_name} must be a non-empty string property")
                    if prop_name == "name":
                        sprite_name = prop["value"]
                    else:
                        animation_names[local_id] = prop["value"]
                elif prop_name == "repeat_count":
                    if prop_type != "int":
                        fail("repeat_count must be an integer property")
                    if "animation" not in metadata.get(local_id, {}):
                        fail("repeat_count requires a tile animation")
                    animation_repeats[local_id] = unsigned_32(prop.get("value"), "repeat_count")
                elif prop_name in ("pivot_x", "pivot_y"):
                    if prop_type != "int":
                        fail(f"{prop_name} must be an integer property")
                    if prop_name == "pivot_x":
                        pivot_x = signed_32(prop.get("value"), prop_name)
                    else:
                        pivot_y = signed_32(prop.get("value"), prop_name)
                elif prop_type == "int":
                    enabled = signed_32(prop.get("value"), f"tile property {prop_name}") != 0
                    if prop_name in flags and enabled:
                        tile_flags |= flags[prop_name]
                elif prop_type == "bool":
                    enabled = boolean(prop.get("value"), f"tile property {prop_name}")
                    if prop_name in flags and enabled:
                        tile_flags |= flags[prop_name]
                else:
                    fail("only integer and boolean tile properties plus reserved TabOS metadata are supported")
            sprite_id = len(assets.sprites)
            assets.sprites.append(Sprite(sprite_name, image_id, x, y, tw, th, pivot_x, pivot_y, tile_flags))
            gid = first_gid + local_id
            if gid in gid_map:
                fail(f"{tileset_label} GID {gid} overlaps another tileset")
            gid_map[gid] = sprite_id
            gid_alignments[gid] = object_alignment
            local_sprite_ids.append(sprite_id)
        for local_id, item in metadata.items():
            if "animation" in item:
                frames_out = []
                animation_values = require_list(item["animation"], f"{tileset_label} tile {local_id} animation")
                for frame_index, frame_value in enumerate(animation_values):
                    frame = require_object(
                        frame_value, f"{tileset_label} tile {local_id} animation[{frame_index}]")
                    frame_id = integer(frame.get("tileid"), "animation tileid")
                    if frame_id < 0 or frame_id >= len(local_sprite_ids):
                        fail("Tiled animation references invalid tile")
                    duration = integer(frame.get("duration"), "animation duration")
                    if duration <= 0:
                        fail("Tiled animation duration must be positive")
                    frames_out.append((local_sprite_ids[frame_id], duration))
                if not frames_out:
                    fail("Tiled animation needs at least one frame")
                animation_name = animation_names.get(local_id, f"{name}_{tileset_name}_{local_id}")
                assets.animations.append(Animation(animation_name, frames_out, animation_repeats.get(local_id, 0),
                                                    local_sprite_ids[local_id]))
    layers = []
    objects = []
    properties = []
    layer_values = require_list(tiled.get("layers", []), f"Tiled map {path} layers")
    for layer_index, layer_value in enumerate(layer_values):
        layer = require_object(layer_value, f"Tiled map {path} layers[{layer_index}]")
        layer_name = require_string(layer, "name")
        if layer.get("type") == "tilelayer":
            if (integer(layer.get("width", width), f"tile layer {layer_name!r} width") != width or
                    integer(layer.get("height", height), f"tile layer {layer_name!r} height") != height):
                fail(f"tile layer {layer_name!r} dimensions must match Tiled map {path}")
            if (integer(layer.get("offsetx", 0), f"tile layer {layer_name!r} offsetx") != 0 or
                    integer(layer.get("offsety", 0), f"tile layer {layer_name!r} offsety") != 0):
                fail(f"tile layer {layer_name!r} offsets are not supported")
            data = require_list(layer.get("data"), f"tile layer {layer_name!r} data")
            if len(data) != width * height:
                fail(f"tile layer {layer_name!r} must contain width * height cells")
            cells = []
            for raw_gid in data:
                gid = unsigned_32(raw_gid, f"tile layer {layer_name!r} GID")
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
            object_values = require_list(layer.get("objects", []), f"object layer {layer_name!r} objects")
            for object_index, object_value in enumerate(object_values):
                obj = require_object(object_value, f"object layer {layer_name!r} objects[{object_index}]")
                object_label = f"Tiled map {path} object {obj.get('name', obj.get('id', object_index))!r}"
                unsupported = any(key in obj for key in ("text", "polygon", "polyline", "template"))
                if "ellipse" in obj:
                    unsupported = boolean(obj["ellipse"], "object ellipse") or unsupported
                if unsupported or obj.get("rotation", 0) != 0:
                    fail(f"object {obj.get('name', obj.get('id'))!r} uses unsupported geometry or rotation")
                tile = 0
                point = boolean(obj.get("point", False), "object point")
                shape = 0 if point else 1
                object_alignment = "topleft"
                if "gid" in obj:
                    raw_gid = unsigned_32(obj["gid"], "object gid")
                    plain_gid = raw_gid & GID_MASK
                    if plain_gid not in gid_map or raw_gid & GID_RESERVED:
                        fail("tile object contains malformed or unknown GID")
                    tile = (raw_gid & (GID_HORIZONTAL | GID_VERTICAL | GID_DIAGONAL)) | (gid_map[plain_gid] + 1)
                    shape = 2
                    object_alignment = gid_alignments[plain_gid]
                object_x = finite_number(obj.get("x", 0), f"{object_label} x")
                object_y = finite_number(obj.get("y", 0), f"{object_label} y")
                object_width = finite_number(obj.get("width", 0), f"{object_label} width")
                object_height = finite_number(obj.get("height", 0), f"{object_label} height")
                if object_width < 0 or object_height < 0:
                    fail(f"{object_label} dimensions must be nonnegative")
                if object_alignment != "topleft":
                    object_x, object_y = tiled_object_origin(
                        object_x, object_y, object_width, object_height, object_alignment)
                geometry = [
                    rounded_integer(object_x, f"{object_label} x", -0x80000000, 0x7FFFFFFF),
                    rounded_integer(object_y, f"{object_label} y", -0x80000000, 0x7FFFFFFF),
                    rounded_integer(object_width, f"{object_label} width", 0, 0xFFFFFFFF),
                    rounded_integer(object_height, f"{object_label} height", 0, 0xFFFFFFFF),
                ]
                first_property = len(properties)
                property_values = require_list(obj.get("properties", []), "object properties")
                for property_index, prop_value in enumerate(property_values):
                    prop = require_object(prop_value, f"object properties[{property_index}]")
                    if prop.get("type") != "int":
                        fail("object properties must be integers")
                    properties.append((require_string(prop, "name"), signed_32(prop.get("value"), "object property")))
                object_id = unsigned_32(obj.get("id"), "object id")
                if object_id == 0:
                    fail("object id must be nonzero")
                object_name = optional_string(obj, "name", "object name")
                object_class = optional_string(obj, "class", "object class")
                if not object_class:
                    object_class = optional_string(obj, "type", "object type")
                objects.append({"id": object_id, "name": object_name,
                                "class": object_class, "shape": shape,
                                "x": geometry[0], "y": geometry[1], "width": geometry[2], "height": geometry[3],
                                "tile": tile, "first_property": first_property,
                                "property_count": len(properties) - first_property})
            layers.append({"name": layer_name, "type": 1, "first": first, "count": len(objects) - first})
        else:
            fail(f"Tiled map {path} uses unsupported layer type {layer.get('type')!r}")
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


def write_header(assets: AssetSet, output: Path, *, include_declarations: bool = True) -> None:
    prefix = identifier(assets.name).lower()
    header = ["/* Generated by tabos assets build. Do not edit. */", "",
              "#ifndef TABOS_ASSETS_" + identifier(assets.name) + "_H",
              "#define TABOS_ASSETS_" + identifier(assets.name) + "_H", ""]
    if include_declarations:
        header.extend(["#include <tabos/tilemap.h>", ""])
    for kind, names in assets.constants.items():
        for name in names:
            value = assets.constant_values[(kind, name)]
            header.append(f"#define {identifier(assets.name)}_{kind}_{identifier(name)} {value}U")
    if include_declarations:
        header.extend(["", f"extern const tabos_sprite_set_t {prefix}_sprites;"])
        for tiled in assets.maps:
            header.append(f"extern tabos_tilemap_t {prefix}_{identifier(tiled['name']).lower()};")
    header.extend(["", "#endif", ""])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(header), encoding="utf-8")


def write_c(assets: AssetSet, output: Path) -> None:
    prefix = identifier(assets.name).lower()
    write_header(assets, output.with_suffix(".h"))
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
    header_output = Path(args.header_output).resolve() if args.header_output else None
    if header_output is not None and header_output != stem.with_suffix(".h"):
        write_header(assets, header_output, include_declarations=False)
    print(f"wrote {stem.with_suffix('.tsp')}")
    for tiled in assets.maps:
        print(f"wrote {output / (tiled['name'] + '.tmap')}")
    print(f"wrote {stem.with_suffix('.c')} and {stem.with_suffix('.h')}")
    if header_output is not None and header_output != stem.with_suffix(".h"):
        print(f"wrote {header_output}")
