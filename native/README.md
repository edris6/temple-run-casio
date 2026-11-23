# Native fx-CG50 port (work in progress)

This directory begins a native C port of the Unity reconstruction at
[`LuciusTran/templerun` commit `65964ad`](https://github.com/LuciusTran/templerun/tree/65964ad).
It does not run Unity or claim that the incomplete reconstruction is the full
original game. The target is an fx-CG50 `.g3a` add-in built with fxSDK and gint.

**Current result: a renderer and motion diagnostic, not a playable game.** It
draws the actual exported runner and temple-path mesh geometry with downsampled
original textures, without skinning, and moves the camera along repeated straight tiles. Up jumps, down
slides, 4 and 6 change lateral position, and EXIT quits. Score and speed use
selected recovered formulas. Obstacles, coins, turns, animation, UI, input
timing, sound, saves and the full scene are not integrated. Camera framing,
depth handling and material handling are temporary. There are no collision
objects in this diagnostic, despite the collision predicates being ported.

## Code provenance

| Native implementation | Reconstructed Unity source |
| --- | --- |
| `tr_reset`, `tr_jump`, `tr_slide`, `tr_stumble`, `tr_add_coins` | `Assets/Scripts/Assembly-CSharp/CharacterPlayer.cs` |
| `tr_step` | straight-running subset of `CharacterPlayer.Recovered.cs`, `MovingObject.Recovered.cs`, and `TempleRunFlow.cs` |
| Acceleration, distance scoring, obstacle guards | `Assets/Restoration/TempleRunRules.cs` and `TempleRunFlow.cs` |
| Polygon and segment predicates | `Assets/Scripts/Assembly-CSharp/Region2D.cs`, in local coordinates |
| 100 start speed, 210 max speed and acceleration values | `Assets/Resources/TempleRunNativeConfig.json` |

These are C translations of selected reconstructed methods, not proven
equivalent to the original binary. `tr_step` currently omits the boost and
power-up branches, raised-ground transitions, lateral motion in distance
calculation, animation and other host integrations. The source course
scheduler was authored for the reconstruction; it is not the recovered
original generator and has not been ported yet. The renderer is new code,
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
exited after a scripted key sequence. The emulator does not establish real
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
were additionally exercised against the reference checkout: 976 vertices / 468
triangles for the runner, and 691 vertices / 402 triangles for the path.

## Next integration work

1. Apply prefab transform hierarchies, material settings and lightmaps; replace
   bind-pose runner rendering with skeleton and animation playback.
2. Port the host track scheduler and the relevant `PathElement` turn rules;
   load the actual corner, obstacle and pickup meshes and collision regions.
3. Wire collisions, ground gaps, score, coins, pause, death and restart into the
   calculator loop; verify behavior against the reconstruction.
4. Profile on a calculator, replace expensive software floating point as needed,
   and test keyboard timing, memory use and visuals on real hardware.
