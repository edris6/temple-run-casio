# Temple Run fx-CG50 port: handoff

The goal is a native port of the reconstructed Unity game, not the earlier
Python or Casio BASIC prototypes. The port is incomplete; see
[native/README.md](native/README.md) for controls, build instructions and source
provenance.

## Junctions and requested obstacles

To give the player more time between actions, the course generator now keeps
at least six tile centres (360 units) between any two hazards, narrow sections
or junctions. Turns due too close to a hazard are delayed. The first narrow,
gap and slide examples are at tiles 17, 23 and 29, with the next junction no
sooner than tile 35. Generator checks cover 100 seeds at three progress values;
a session test exercises a slide followed by a jump at maximum speed. Physical
device and real-player reaction times remain unverified.

The first and subsequent generated corners are now T junctions: either arrow
selects the corresponding exit. The already-generated continuation is rotated
when needed, so both choices reuse the same course and assets. The junction
renderer combines existing corner meshes, retaining the near walls of both
exits; a host regression checks the split for all four approach directions.

After the first junction, authored placements guarantee a narrow section,
then a gap, then a slide obstacle, each separated by clear tiles. Left/right
steer on straights and choose a direction near a junction, up jumps and down
slides. The slide posture is a simple mesh deformation, not the original
skeletal animation. Collision and D-pad session checks cover these mechanics;
the emulator was exercised through the first junction in both directions, but
the complete obstacle sequence still needs an emulator and real-device playtest.
The same temple scenery is used throughout.

## Renderer and display optimization

The flat temple-floor faces now use approximate perspective-correct texture
mapping. The earlier affine mapping visibly distorted nearby paving stones.
Other faces, including coins, retain their previous mapping. This applies to
flat faces at prefab-local height −9; irregular hole surfaces still use affine
mapping. The row-wise correction assumes a level, unrolled camera and is an
approximation during integer projection and turns.

In the same four-second emulator scenario described below, a diagnostic build
with the floor correction displayed about 41 frames versus about 48 without it.
This is an emulator-only estimate; physical performance remains unknown.

The triangle loop now uses 32-bit wrapping accumulators, a 25 KB reciprocal
cache avoids repeated projection divisions, and the display code initializes
both gint VRAM buffers once and writes aligned pairs of pixels. Resolution and
camera are unchanged. The cache increases static RAM use; the linked add-in has
193,584 bytes of BSS plus other runtime memory.

The rasterizer's output matched the previous version across 4,000 generated
textured triangles, extreme projected triangles, and six host-rendered course
views. The cached quotient was checked against ordinary division across every
supported depth, including signed extremes. Paused emulator screenshots matched
the previous build byte for byte; the turn, pause, resume, death and restart
scenarios also ran in the emulator.

With the same fixed-clock emulator setup, an instrumented four-second run
displayed about 48 frames versus 41 before the optimization. This is a small
sample from the emulator's timing model, not a physical-device FPS measurement.
Physical VRAM/DMA behavior and performance still need a calculator test.

## Current state

- Imported original meshes are deduplicated and transformed into fixed-point
  coordinates. The renderer uses a 192×108 internal framebuffer, scaled 2× to
  the screen. Its closer camera retains the reference's downward pitch while
  making the runner easier to see.
- Left/right move sideways on straight paths and request a turn near a corner;
  up jumps, down slides, 4/6 also move sideways, F1 pauses and EXIT quits.
- Straight, corner, gap and narrow-path ground polygons come from the temple
  prefabs. Ground queries run after movement and support jumping gaps and falling.
  Missed corners still cause immediate death rather than a falling animation.
- The runner uses its bind pose with an approximate slide posture. Animation, scenery, original UI, audio,
  saves, power-ups, and other game features remain unfinished.

## Verification and limits

- With a fixed clock, `native/tests/emulator-turn.txt` cleared the opening
  tree and first left turn in the fx-CG50 add-in emulator. An instrumented
  temporary setup confirmed arrow-key sideways movement, a gap jump and a
  fall when the same gap was not jumped.
- In one instrumented four-second emulator run, the current floor correction
  displayed roughly 41 frames, versus roughly 48 immediately before it. This
  is not a physical-device FPS measurement.
- Native host checks run with `make -C native test`; Python asset-converter
  checks run with `python3 -m unittest discover -s tests -v`. The C checks were
  also run with AddressSanitizer and UndefinedBehaviorSanitizer.
- The add-in cross-compiles with fxSDK/gint. No physical calculator or hardware
  performance has been verified. The ignored local `native/TRUN3D.g3a` and
  generated headers contain derived original assets and must not be redistributed.

## Next work

1. Test the current build on an fx-CG50. Profile rendering and input across
   different scenes; the emulator's timing model is approximate.
2. Import and bake the actual runner skeleton and animation clips. The runner
   prefab references the uncompressed `Assets/AnimationClip/Run_0.anim` by GUID
   `5258da0604b9bea4fa9a3a3a04473074`. Its mesh has bone weights in vertex stream
   2; the current deduplication only considers position and UV, so preserve skin
   weights when extending the importer.
3. Verify ground, turns and collisions across headings and seeds; port falling
   and death presentation, and improve camera tracking during jumps and turns.
4. Add original UI, environment, effects, pickups and saves while keeping source
   provenance and authored approximations clear.

The sparse reference checkout was `/tmp/templerun-unity-reference` at commit
`65964ad`. The separate emulator was `/tmp/temple-emulator`. Both are temporary
and may be cleared. The cross-build used Docker image `manawyrm/fxsdk`, digest
`sha256:b5283be1ba98d90aadc2cbc767d0424baa801344c71108c587e922b37392beaf`.
