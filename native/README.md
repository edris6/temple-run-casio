# Native fx-CG50 port (work in progress)

**Latest source status:** connected-turn and renderer performance work is in
progress and is not yet validated in the calculator emulator. The first-turn
emulator test failed before the latest optimization. The details below describe
the earlier stable opening-course checkpoint; see [HANDOFF.md](../HANDOFF.md)
for the current state and next checks.

This directory begins a native C port of the Unity reconstruction at
[`LuciusTran/templerun` commit `65964ad`](https://github.com/LuciusTran/templerun/tree/65964ad).
It does not run Unity or claim that the incomplete reconstruction is the full
original game. The target is an fx-CG50 `.g3a` add-in built with fxSDK and gint.

**Current result: the opening straight is playable; the full game is not yet
ported.** The runner, temple path, coins, jump trees and trip trees use their
exported meshes and downsampled textures. The first corner's original mesh is
displayed, but turning through it is not implemented yet. Course placement, coin pickup bounds
and obstacle collision regions come from the reference reconstruction. Up jumps,
down slides, 4 and 6 change lateral position, F1 pauses, and EXIT quits. After
death, EXE restarts. The current run stops at the first corner; EXE restarts
there too. New runs use the calculator clock as a seed.

The character remains in its bind pose. Connected corners, later course types,
animation, original UI, sound, saves and the full scene are not integrated.
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
| Coin radius and player bounds | `TempleRunRules.PickupRadius` and `CharacterPlayer.GetWorldSpaceBounds` |
| 100 start speed, 210 max speed and acceleration values | `Assets/Resources/TempleRunNativeConfig.json` |

These are C translations of selected reconstructed methods, not proven
equivalent to the original binary. `tr_step` currently omits the boost and
power-up branches, raised-ground transitions, lateral motion in distance
calculation, animation and other host integrations. The source course
scheduler was authored for the reconstruction; it is not the recovered
original generator. Only its first 12 tiles are generated here. The renderer is new code,
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

For a host-rendered still, after generating the two mesh headers:

```sh
make build/preview
build/preview build/frame.ppm
```

`make test` runs native C checks for motion, scoring, geometry and the renderer.
From the project root, `python3 -m unittest discover -s tests -v` also checks
the mesh converter against synthetic fixtures. The runner and path converters
were additionally exercised against the reference checkout, including the
runner, path, coin and three obstacle meshes. The emulator was also exercised
through the opening course with visible coins and a first-corner stop.
`tests/emulator-opening.txt` and `tests/emulator-pause.txt` are the manual
emulator scripts, using `/work` for the mounted `native/` directory and a
fixed emulator clock of `2026-01-01 00:00`. They require a separate
emuCasioCalc build; they are not part of `make test`.

## Next integration work

1. Apply prefab transform hierarchies, material settings and lightmaps; replace
   bind-pose runner rendering with skeleton and animation playback.
2. Extend the host track scheduler beyond the opening, and port the relevant
   `PathElement` turn rules and connected corner meshes.
3. Add ground gaps, original pause and death panels, and other pickups; verify
   behavior against the reconstruction.
4. Profile on a calculator, replace expensive software floating point as needed,
   and test keyboard timing, memory use and visuals on real hardware.
