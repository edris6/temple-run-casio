# Temple Run fx-CG50 port: handoff

The goal is a native port of the reconstructed Unity game, not the earlier
Python or Casio BASIC prototypes. The port is incomplete; see
[native/README.md](native/README.md) for controls, build instructions and source
provenance.

## Paused optimization investigation

The user asked to stop. No optimization experiment was applied to tracked
source, and `native/TRUN3D.g3a` is still the previous 192×108 build from commit
`b421914`. Do not treat the temporary builds as verified replacements.

An isolated emulator copy sampled the opening run's emulated instruction
addresses. Of samples mapped to code, about 40% landed in triangle rasterization,
18% in `dupdate`, and about 10% in mesh handling. Division routines accounted
for additional samples. This is one emulator workload, not a hardware profile.
The ignored `native/build/profile.txt` and `native/build/symbols.txt` contain
the raw sample buckets and symbol list; `/tmp/temple-prof` holds the modified
profiling emulator. Temporary directories may be deleted by the operating system.

Experiments in `/tmp` only:

- `/tmp/renderer-32bit.c` uses wrapping 32-bit pixel accumulators instead of
  64-bit additions in the triangle loop. A randomized host comparison produced
  the same hash for 200 frames of synthetic triangles, and the short emulator
  benchmark displayed about 22 frames in two seconds versus about 20 before.
  This is preliminary; test numeric bounds and additional scenes before use.
- `/tmp/renderer-depth-exact.c` adds a roughly 25 KB reciprocal-depth cache and
  corrects the estimated quotient to match integer division. One desktop preview
  was byte-identical; the short emulator benchmark displayed about 23 frames.
  Arithmetic limits, memory cost and real-device behavior remain unverified.
- `/tmp/temple-bench/src/cg_main.c` contains an instrumented experiment that
  initializes both gint display buffers once and writes pairs of pixels with
  aligned 32-bit stores. The no-clear stage showed about 24 frames in the same
  short benchmark; the final paired-store result and pause, restart, borders,
  and physical DMA behavior were not verified. `gint/display.h` documents the
  CG50's buffer switching and 32-byte VRAM alignment.

Next, resume by testing each change independently against the tracked build,
including randomized numerical edge cases, screenshots across turns and gaps,
pause/restart behavior and repeated timing runs. Keep the 192×108 resolution and
closer camera requested by the user. No physical-device performance is known.

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
- The runner is still in its bind pose. Animation, scenery, original UI, audio,
  saves, power-ups, and other game features remain unfinished.

## Verification and limits

- With a fixed clock, `native/tests/emulator-turn.txt` cleared the opening
  tree and first left turn in the fx-CG50 add-in emulator. An instrumented
  temporary setup confirmed arrow-key sideways movement, a gap jump and a
  fall when the same gap was not jumped.
- In one instrumented two-second emulator run, the current higher-resolution
  opening displayed roughly 20 frames, versus roughly 30 at the preceding
  128×72 setting. This is not a physical-device FPS measurement.
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
