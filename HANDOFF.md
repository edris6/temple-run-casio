# Temple Run fx-CG50 port: handoff

The goal is a native port of the reconstructed Unity game, not the earlier
Python or Casio BASIC prototypes. The port is incomplete; see
[native/README.md](native/README.md) for controls, build instructions, the
rendering stages and source provenance.

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

## Performance work (November 2026)

Emulator results, comparing commit `4ee6a4b` with the optimised renderer.
Both used the fx-CG50 model, `--timing realistic` and a fixed clock.

| Measurement | Before | After |
| --- | --- | --- |
| Scripted play, about 28 s: average frames per second | 12.9 | 57.4 |
| Scripted play: median / worst frame time | 74.5 / 105 ms | 16.9 / 23.3 ms |
| 600 identical game states, drawn back to back: average frames per second | 12.0 | 55.8 |
| Same run: instructions per frame | 9.7 million | 1.9 million |

The emulator charges one cycle per instruction plus estimates for uncached
memory and screen DMA. It has **no model of caches, pipeline or multiply
latency**, so these are not calculator frame rates. A separate rough estimate
added an assumed 32 kB, 4-way cache with 30 cycles per miss and write-back; it
gave 11.3 before and 46.4 after. Treat both as relative evidence until F6 has
been read on an fx-CG50.

Where the time went before, and what changed:

- `dupdate()` blocked for the whole screen transfer, about 11 ms, because gint
  uses a single video buffer unless a second is set. The transfer now runs by
  DMA while the next frame is drawn; the program waits only before writing
  video RAM again.
- 64-bit software division in the rasterizer took about 16% of instructions.
  It now uses small reciprocal tables.
- Struct copies in the renderer became `memcpy` calls, mostly spent in its
  byte-by-byte loop (about 9%). The build uses `-O2` and passes pointers.
- Back faces are culled (except in the assembled junction), meshes outside the
  view are skipped, equal positions are transformed once, and triangles whose
  bounding box holds at most four pixel centres take one texture sample.
- Pixel rows, the screen clear and the 2× upscale are in SH-4 assembly.
- Meshes and textures are copied to RAM, and distant surfaces use smaller
  texture levels. These and `movca.l` in the clear aim at real-hardware memory
  costs that the emulator cannot show.
- The 30 Hz simulation is unchanged. Frames are placed between the current
  step and a prediction of the next, so motion advances on every frame.

Visible differences: distant textures are smoother and lose some fine detail
(for example the thin red trim on far walls), and silhouettes differ by single
pixels because sampling now uses pixel centres. Level thresholds are in
`raster.c`.

Verification: host tests also pass under AddressSanitizer and
UndefinedBehaviorSanitizer. The check add-in matched assembly and C results in
the emulator, and failed when bugs were deliberately planted in the assembly.
Over the 600-state run, colour and depth of every frame hashed identically in
the calculator build and the host build. The benchmark add-in, frame-logging
emulator patch and analysis scripts were temporary and are not in the
repository.

Not done: CPU overclocking (gint's `clock_set_speed`) is not used, because it
affects battery life and hardware margins. If real hardware is still too slow,
that is one option; further candidates are drawing in screen bands to reduce
cache misses and writing video RAM through the cache.

## Current state

- Imported original meshes are transformed into fixed-point coordinates by the
  converter. The renderer uses a 192×108 internal framebuffer, scaled 2× to
  the screen. Its closer camera retains the reference's downward pitch while
  making the runner easier to see.
- Left/right move sideways on straight paths and request a turn near a corner;
  up jumps, down slides, 4/6 also move sideways, F1 pauses, F6 shows the frame
  rate and EXIT quits.
- Straight, corner, gap and narrow-path ground polygons come from the temple
  prefabs. Ground queries run after movement and support jumping gaps and falling.
  Missed corners still cause immediate death rather than a falling animation,
  with only the runner left in view.
- The runner uses its bind pose with an approximate slide posture. Animation,
  scenery, original UI, audio, saves, power-ups, and other game features remain
  unfinished.

## Verification and limits

- With a fixed clock, the current build ran the four `native/tests/emulator-*.txt`
  scripts. The turn script cleared the opening tree and first left turn, and a
  copy using RIGHT cleared it too; two paused screenshots were identical.
- Earlier builds confirmed arrow-key sideways movement, a gap jump and a fall in
  an instrumented emulator setup. That was not repeated.
- Native host checks run with `make -C native test`; Python asset-converter
  checks run with `python3 -m unittest discover -s tests -v`.
- The add-in cross-compiles with fxSDK/gint. No physical calculator or hardware
  performance has been verified. The ignored local `native/TRUN3D.g3a` and
  generated headers contain derived original assets and must not be redistributed.

## Next work

1. Test the current build on an fx-CG50: note the F6 frame rate in several
   scenes, check input timing, and run `TRCHECK.g3a`.
2. Import and bake the actual runner skeleton and animation clips. The runner
   prefab references the uncompressed `Assets/AnimationClip/Run_0.anim` by GUID
   `5258da0604b9bea4fa9a3a3a04473074`. Its mesh has bone weights in vertex stream
   2. Both the converter (position and UV) and `tr_mesh_build` (position only)
   merge vertices, so preserve skin weights in both.
3. Verify ground, turns and collisions across headings and seeds; port falling
   and death presentation, and improve camera tracking during jumps and turns.
4. Add original UI, environment, effects, pickups and saves while keeping source
   provenance and authored approximations clear.

The reference checkout used to generate the headers in `native/build/` was at
commit `65964ad`; its temporary copy in `/tmp` is gone, so re-clone it before
running the converter again. The separate emulator was `/tmp/temple-emulator`,
also temporary. The cross-build used Docker image `manawyrm/fxsdk`, digest
`sha256:b5283be1ba98d90aadc2cbc767d0424baa801344c71108c587e922b37392beaf`.
