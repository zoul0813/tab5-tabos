# Getting Started with the TabOS Tile Engine

This guide teaches the TabOS Tile Engine from the beginning. You do not need prior
experience with TabOS graphics, Tiled, sprite sheets, or tile maps.

By the end, you will know how to:

- create a map and tileset in [Tiled](https://www.mapeditor.org/);
- turn the source assets into TabOS runtime files;
- use generated names instead of numeric IDs;
- load and draw a scrolling map;
- draw sprites, animations, and metasprites between map layers;
- use tile flags and object layers for game behavior; and
- change map cells while the game is running.

The guide uses the recommended binary-backed asset workflow. The converter creates a
sprite package (`.tsp`), one map package (`.tmap`) for each map, and a C header containing
the names used by game code.

## How the Engine Fits Together

The Tile Engine is a small rendering and asset layer in the TabOS application SDK:

```text
PNG/GIF + Tiled TMJ/TSJ + manifest.json
                    |
                    | tabos assets build
                    v
       generated IDs + .tsp + .tmap
                    |
                    | load once
                    v
       game state -> draw calls -> present
```

Tiled owns authored content: image regions, map cells, layer order, tile animation frames,
object markers, and integer properties. The application owns game state: player movement,
collision rules, enemies, animation transitions, camera movement, and object behavior.

The engine does not create entities or automatically draw object layers. This keeps the
API direct: the game asks for exactly the layers and actors it wants to draw each frame.

## Learning Path

Read these pages in order the first time:

1. [Core Concepts](concepts.md) explains the vocabulary and coordinate system.
2. [Author Assets in Tiled](authoring.md) prepares the files and first tileset and map.
3. [Work with Tiled](tiled-workflow.md) gives detailed editor examples for metadata, animations, layers, objects, and transforms.
4. [Build and Load Assets](build-assets.md) connects asset generation to an application.
5. [Complete First Application](first-app.md) provides a buildable map viewer.
6. [Draw the Game](draw-game.md) explains the render loop, camera, layers, sprites, and animation.
7. [Add Gameplay](gameplay.md) covers collision flags, objects, properties, metasprites, and editable cells.
8. [Troubleshooting](troubleshooting.md) explains common symptoms and fixes.

After the tutorial, use the [Sprite and Tile Assets manual](../../tile-assets.md) as the
complete feature reference. The public API contracts are documented directly in
[`<tabos/sprite.h>`](../../../sdk/include/tabos/sprite.h) and
[`<tabos/tilemap.h>`](../../../sdk/include/tabos/tilemap.h).

## Working Example

The repository includes a complete example in [`apps/tile-demo`](../../../apps/tile-demo).
Its executable is named `tdemo`. It demonstrates:

- a Tiled map with ground, foreground, and object layers;
- a looping robot animation;
- sprite pivots and all eight Tiled tile transforms;
- camera scrolling;
- tile flags used for collision;
- named point, rectangle, and tile objects;
- an integer object property;
- an ordered tree-and-shadow metasprite; and
- a tile changed at runtime.

Build its application and assets with:

```sh
make -C apps/tile-demo
```

Open [`world.tmj`](../../../apps/tile-demo/assets/world.tmj) in Tiled to compare the
authored map with [`main.c`](../../../apps/tile-demo/src/main.c). The detailed controls
and behavior are listed in the [`tdemo` application documentation](../../applications.md#building-applications).

## Reference Map

Use these manual sections when you need exact behavior:

| Need | Reference |
| --- | --- |
| Sprite, animation, and tilemap functions | [Runtime API](../../tile-assets.md#runtime-api) |
| Small C recipes | [Game Recipes](../../tile-assets.md#game-recipes) |
| Manifest, PNG/GIF, and Tiled fields | [Authoring](../../tile-assets.md#authoring) |
| Host validation | [Validation](../../tile-assets.md#validation) |
| Application asset installation | [Installation](../../tile-assets.md#installation) |
| Camera and screen coordinates | [Graphics API](../../graphics-api.md) |
| Keyboard events | [Keyboard Input](../../input.md) |
| Application build and lifecycle | [Application Lifecycle](../../applications.md) |
