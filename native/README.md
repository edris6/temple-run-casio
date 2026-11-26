# Native fx-CG50 port (work in progress)

**Latest status:** the first junction offers both directions, followed by a
narrow section, a gap and a low obstacle. Fixed-clock emulator runs of the
current build have exercised the opening jump, both junction choices, pause,
death and restart; the other mechanics have host-level input and collision
tests. Physical-calculator behavior remains unverified. See
[HANDOFF.md](../HANDOFF.md).

This directory begins a native C port of the Unity reconstruction at
[`LuciusTran/templerun` commit `65964ad`](https://github.com/LuciusTran/templerun/tree/65964ad).
It does not run Unity or claim that the incomplete reconstruction is the full
original game. The target is an fx-CG50 `.g3a` add-in built with fxSDK and gint.

## Controls

| Key | Action |
| --- | --- |
| Up / Down | Jump / slide |
| Left / Right | Move sideways on straight paths, turn near corners |
| 4 / 6 | Move sideways |
| F1 | Pause and resume |
| F6 | Show or hide the frame rate |
| EXE | Restart after death |
| EXIT | Quit |

New runs use the calculator clock as a seed.

## Current diagnostic

The runner, paths, corners, coins and trees use exported meshes and downsampled
textures. Course placement, coin pickup bounds and obstacle collision regions
come from the reference reconstruction.

The bounded course generator continues past junctions, and ground checks cover
gaps and narrow paths. Both junction choices lead to the same generated course
rotated into the selected direction. The first examples of the narrow path,
gap and slide obstacle are deliberately placed after the first junction; this
placement is authored for the port. The runner is in its bind pose except for
an approximate compressed slide posture. Original skeletal animation,
original UI, sound, saves and the full scene are not integrated.
Input timing and performance still need physical-device testing. Camera framing,
depth handling and material handling are temporary.

The port spaces every generated hazard, narrow section and junction at least
six 60-unit tiles apart. At maximum speed, the tested slide-to-gap case leaves
at least 0.8 seconds after the slide finishes before the gap starts. This is a
course-generation guarantee, not a measurement of a player's reaction time.

## Code provenance

| Native implementation | Reconstructed Unity source |
| --- | --- |
| `tr_reset`, `tr_jump`, `tr_slide`, `tr_stumble`, `tr_add_coins` | `Assets/Scripts/Assembly-CSharp/CharacterPlayer.cs` |
| `tr_step` | straight-running subset of `CharacterPlayer.Recovered.cs`, `MovingObject.Recovered.cs`, and `TempleRunFlow.cs` |
| Acceleration, distance scoring, obstacle guards | `Assets/Restoration/TempleRunRules.cs` and `TempleRunFlow.cs` |
| Polygon and segment predicates | `Assets/Scripts/Assembly-CSharp/Region2D.cs`, in local coordinates |
| Opening tile placements, coin positions and corner choice | `Assets/Restoration/TempleRunFlow.Track.cs` |
| Obstacle polygons, collision heights and lethal flags | `treeJumpLeftVert`, `treeTripLeftVert` and `treeSlideVert` prefab data |
| Hole and narrow path meshes and ground polygons | `templeStraightHole{Vert,Horz}` and `templeStraightNarrowSmall{LeftVert,UpHorz}` prefabs |
| Coin radius and player bounds | `TempleRunRules.PickupRadius` and `CharacterPlayer.GetWorldSpaceBounds` |
| 100 start speed, 210 max speed and acceleration values | `Assets/Resources/TempleRunNativeConfig.json` |

These are C translations of selected reconstructed methods, not proven
equivalent to the original binary. `tr_step` currently omits the boost and
power-up branches, raised-ground transitions, lateral motion in distance
calculation, animation and other host integrations. The source course
scheduler was authored for the reconstruction; it is not the recovered
original generator. The junction and initial obstacle sequence are authored
adaptations. The renderer is new code, not a Unity engine implementation.
Original lightmaps and full Unity material behavior are not used.

## How a frame is made

| Stage | File | What it does |
| --- | --- | --- |
| Pacing | `cg_main.c` | Simulates at 30 Hz, draws up to 60 frames a second, and places each frame between the current step and the predicted next one |
| Scene and vertices | `renderer.c` | Picks nearby tiles, skips meshes outside the view, transforms each distinct position once in integer arithmetic, clips at the near plane |
| Triangle setup | `raster.c` | Rejects hidden and back-facing triangles, picks a 128, 64 or 32 texel texture level, and fills triangles of a few pixels with one sample |
| Pixel rows | `sh4.S`, `rows.c` | Depth test and texture lookup in assembly, with a C equivalent |
| Screen | `present.c`, `sh4.S` | Doubles the 192×108 frame into video RAM, which DMA sends while the next frame is drawn |

The temple floor is texture-mapped with perspective correction (the camera is
level, so one depth serves a whole screen row). Other surfaces use affine
mapping, which can warp large nearby faces. Back faces are culled everywhere
except in the assembled junction. The CG50 has no FPU or divide instruction:
the renderer uses reciprocal tables, while the game logic still uses software
floating point.

`assets.c` copies meshes and textures from the add-in's ROM into RAM at startup
and builds the smaller texture levels. Static RAM use is about 376 kB of the
491 kB available to the add-in.

## Build

Obtain your own checkout of the reference repository at commit `65964ad`.
The original assets are not tracked in this repository. The converter reads
uncompressed Unity YAML mesh vertices, UV0 and indices, deduplicates identical
position/UV pairs, bakes selected prefab transforms, and converts PNG textures
to 128x128 RGB565. It does not yet import material parameters, arbitrary prefab
transforms, skeletons or animation. Its output and the
resulting diagnostic binary contain derived game assets: keep them local and
do not redistribute them. Generated outputs are ignored by Git.

From this directory, replacing `REFERENCE` with the checkout path:

```sh
mkdir -p build
python3 tools/import_assets.py REFERENCE
python3 tools/make_icons.py
fxsdk build-cg
```

The build writes `TRUN3D.g3a`. An fxSDK/gint cross-toolchain is required.
The same build was tested using the `manawyrm/fxsdk` container image, digest
`sha256:b5283be1ba98d90aadc2cbc767d0424baa801344c71108c587e922b37392beaf`,
with networking disabled during compilation. For example, from this directory:

```sh
docker run --platform linux/amd64 --network none --rm \
  -v "$PWD:/work" -w /work manawyrm/fxsdk fxsdk build-cg
```

With a cable, Casio add-ins normally go in storage memory; back up important
data first. Treat this output as a diagnostic build, not a release.

For a host-rendered still, after generating the asset headers (the optional
number runs that many simulation steps first, without input):

```sh
make build/preview
build/preview build/frame.ppm 60
```

## Tests

`make test` runs native C checks for motion, scoring, course generation, input,
geometry and the renderer. The rasterizer is compared with double-precision
references for coverage, shared edges, depth and texture coordinates. From the
project root, `python3 -m unittest discover -s tests -v` also checks the asset
converters against synthetic fixtures.

The host tests run the C forms of the pixel loops. To compare them with the
assembly on the calculator (or an emulator), build the check add-in, run
`TRCHECK.g3a`, and expect three `OK` lines. It contains no game assets.

```sh
docker run --platform linux/amd64 --network none --rm \
  -v "$PWD:/work" -w /work manawyrm/fxsdk \
  sh -c 'fxsdk build-cg && cmake --build build-cg --target temple_check'
```

## Emulator checks

The add-in is run in the open-source
[emuCasioCalc](https://github.com/joserodpt/emuCasioCalc) fx-CG50 add-in
emulator. The emulator does not establish real calculator performance or
complete input correctness, and no physical calculator has been tested.

`tests/emulator-opening.txt`, `tests/emulator-pause.txt`,
`tests/emulator-turn.txt` and `tests/emulator-restart.txt` are manual key
scripts, using `/work` for the mounted `native/` directory and a fixed emulator
clock of `2026-01-01 00:00`. They require a separate emuCasioCalc build and are
not part of `make test`. The opening script never turns, so its final
screenshot shows the run ending at the first junction. The scripts depend on
timing and on the clock-derived seed.

## Next integration work

1. Profile on a calculator (F6 shows the frame rate) and check keyboard timing,
   memory use and visuals on real hardware.
2. Apply prefab transform hierarchies, material settings and lightmaps; replace
   bind-pose runner rendering with skeleton and animation playback.
3. Verify the connected course, controls, ground checks and camera across
   directions and seeds against the reconstruction.
4. Add original pause and death panels and other pickups.
