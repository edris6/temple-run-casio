# Native fx-CG50 port (work in progress)

**Latest source status:** one scripted emulator run jumps the opening tree,
turns left at the first corner and continues on the connected path. This is
an incomplete diagnostic: rendering is slow, turn timing remains sensitive,
and holes and narrow terrain stop the run. See [HANDOFF.md](../HANDOFF.md).
Their source meshes and ground polygons are imported, but their collision
and fall behavior is not yet connected to gameplay.

This directory begins a native C port of the Unity reconstruction at
[`LuciusTran/templerun` commit `65964ad`](https://github.com/LuciusTran/templerun/tree/65964ad).
It does not run Unity or claim that the incomplete reconstruction is the full
original game. The target is an fx-CG50 `.g3a` add-in built with fxSDK and gint.

## Current diagnostic

The runner, paths, corners, coins and trees use exported meshes and downsampled
textures. Course placement, coin pickup bounds and obstacle collision regions
come from the reference reconstruction. Up jumps, down slides, left and right
turn, 4 and 6 change lateral position, F1 pauses, and EXIT quits. After death,
EXE restarts; new runs use the calculator clock as a seed.

The bounded course generator continues past corners, but the character remains
in its bind pose. Later course types, animation, original UI, sound, saves and
the full scene are not integrated.
Input timing and performance still need physical-device testing. Camera framing,
depth handling and material handling are temporary. The slide-tree mesh and
collision rule are imported and tested, but the reference's initial course
generation does not place slide trees on this opening straight.

## Code provenance

| Native implementation | Reconstructed Unity source |
| --- | --- |
| `tr_reset`, `tr_jump`, `tr_slide`, `tr_stumble`, `tr_add_coins` | `Assets/Scripts/Assembly-CSharp/CharacterPlayer.cs` |
| `tr_step` | straight-running subset of `CharacterPlayer.Recovered.cs`, `MovingObject.Recovered.cs`, and `TempleRunFlow.cs` |
| Acceleration, distance scoring, obstacle guards | `Assets/Restoration/TempleRunRules.cs` and `TempleRunFlow.cs` |
| Polygon and segment predicates | `Assets/Scripts/Assembly-CSharp/Region2D.cs`, in local coordinates |
| Opening tile placements, coin positions and corner choice | `Assets/Restoration/TempleRunFlow.Track.cs` |
| Obstacle polygons, collision heights and lethal flags | `treeJumpLeftVert`, `treeTripLeftVert` and `treeSlideVert` prefab data |
| Hole and narrow path meshes and ground polygons | `templeStraightHole{Vert,Horz}` and `templeStraightNarrowSmall{LeftVert,UpHorz}` prefabs (polygons not yet used in play) |
| Coin radius and player bounds | `TempleRunRules.PickupRadius` and `CharacterPlayer.GetWorldSpaceBounds` |
| 100 start speed, 210 max speed and acceleration values | `Assets/Resources/TempleRunNativeConfig.json` |

These are C translations of selected reconstructed methods, not proven
equivalent to the original binary. `tr_step` currently omits the boost and
power-up branches, raised-ground transitions, lateral motion in distance
calculation, animation and other host integrations. The source course
scheduler was authored for the reconstruction; it is not the recovered
original generator. The latest source extends the scheduler beyond the first
12 tiles, but its turning needs further verification across directions and
seeds. The renderer is new code,
not a Unity engine implementation: it supports position-only triangle meshes,
affine UV mapping, a reciprocal-depth buffer and near-plane clipping. Original
lightmaps and full Unity material behavior are not used.

## Build

Obtain your own checkout of the reference repository at commit `65964ad`.
The original assets are not tracked in this repository. The converter reads
uncompressed Unity YAML mesh vertices, UV0 and indices, and converts PNG textures
to 128x128 RGB565. It does not yet import material parameters, prefab transforms,
skeletons or animation. Its output and the
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

The compiled add-in was run headlessly in the open-source
[emuCasioCalc](https://github.com/joserodpt/emuCasioCalc) fx-CG50 add-in
emulator. It displayed the imported meshes and textures, updated the score and
exited after a scripted key sequence. A later scripted run showed coins,
obstacle collision and death, and pause/resume; two frames taken two seconds
apart while paused were identical. The test also exposed a missed jump when
several simulation ticks accumulated during rendering; applying queued input
before those ticks allowed the scripted jump to clear the tree. The emulator does not establish real
calculator performance or complete input correctness, and no physical
calculator has been tested. With a cable, Casio add-ins normally go in storage
memory; back up important data first. Treat this output as a diagnostic build,
not a release.

For a host-rendered still, after generating the asset headers:

```sh
make build/preview
build/preview build/frame.ppm
```

`make test` runs native C checks for motion, scoring, geometry and the renderer.
From the project root, `python3 -m unittest discover -s tests -v` also checks
the mesh converter against synthetic fixtures. The runner and path converters
were additionally exercised against the reference checkout, including the
runner, path, coin and three obstacle meshes. The emulator was also exercised
through the opening course and one left turn. `tests/emulator-opening.txt`,
`tests/emulator-pause.txt`, and `tests/emulator-turn.txt` are the manual
emulator scripts, using `/work` for the mounted `native/` directory and a
fixed emulator clock of `2026-01-01 00:00`. They require a separate
emuCasioCalc build; they are not part of `make test`.

## Next integration work

1. Apply prefab transform hierarchies, material settings and lightmaps; replace
   bind-pose runner rendering with skeleton and animation playback.
2. Verify and fix the in-progress connected course, turn input and camera;
   then add the remaining ground regions and terrain types.
3. Add ground gaps, original pause and death panels, and other pickups; verify
   behavior against the reconstruction.
4. Profile on a calculator, replace expensive software floating point as needed,
   and test keyboard timing, memory use and visuals on real hardware.
