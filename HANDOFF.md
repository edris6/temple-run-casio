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

## Performance and resolution (November 2026)

Emulator results for three builds: the original renderer (`4ee6a4b`, 192×108),
the optimised renderer (`665c74e`, 192×108) and the current full-screen one
(396×224, about 4.3 times as many pixels). All used the fx-CG50 model,
`--timing realistic` and a fixed clock.

| Measurement | Original | Optimised, 192×108 | Current, 396×224 |
| --- | --- | --- | --- |
| Scripted play, about 28 s: average frames per second | 12.9 | 57.4 | 27.1 |
| Scripted play: median / worst frame time, ms | 74.5 / 105 | 16.9 / 23.3 | 35.6 / 50.1 |
| 600 identical game states, drawn back to back: frames per second | 12.0 | 55.8 | 25.0 |
| Same run: instructions per frame, millions | 9.7 | 1.9 | 4.3 |
| Same run with an assumed cache cost: frames per second | 11.3 | 46.4 | 21.9 |

The emulator charges one cycle per instruction plus estimates for uncached
memory and screen DMA. It has **no model of caches, pipeline or multiply
latency**, so these are not calculator frame rates. The last row comes from a
separate rough estimate with an assumed 32 kB, 4-way cache costing 30 cycles
per miss and write-back. Treat everything as relative evidence until F6 has been
read on an fx-CG50.

The optimised renderer fixed these costs in the original:

- `dupdate()` blocked for the whole screen transfer, about 11 ms, because gint
  uses a single video buffer unless a second is set. The transfer now runs by
  DMA while the next frame is prepared; the program waits only before writing
  video RAM again.
- 64-bit software division in the rasterizer took about 16% of instructions.
  It now uses small reciprocal tables.
- Struct copies in the renderer became `memcpy` calls, mostly spent in its
  byte-by-byte loop (about 9%). The build uses `-O2` and passes pointers.
- Back faces are culled (except in the assembled junction), meshes outside the
  view are skipped, equal positions are transformed once, and triangles whose
  bounding box holds at most four pixel centres take one texture sample.
- Pixel rows, the screen clear and the screen copy are in SH-4 assembly.
- Meshes and textures are copied to RAM, and distant surfaces use smaller
  texture levels. These and `movca.l` in the clear aim at real-hardware memory
  costs that the emulator cannot show.
- The 30 Hz simulation is unchanged. Frames are placed between the current
  step and a prediction of the next, so motion advances on every frame.

### Full resolution

The view is framed as before, with the former black border now filled. A frame
is transformed once into a triangle list, then drawn in 16-row strips. Tests
check that strips give exactly the pixels and depths of drawing the frame at
once. The most triangles seen in a frame was 1926 (60 course seeds), against a
list size of 2304; any excess is silently left out.

Strip height trades repeated triangle setup against cache use. In the 600-state
run, 56-row strips gave 28.7 frames per second in the emulator and 20.9 with
the cache estimate; 16-row strips gave 25.0 and 21.9, and leave about 60 kB
more RAM free. 16 was chosen for the real calculator's cache; compare both
with F6 if speed matters. Drawing two strips before the first wait for the
screen gained about 3% in the emulator and nothing in the estimate, so it was
not kept.

The 128-texel textures now look blocky near the camera. Textures twice the size
would not fit in the remaining RAM.

Verification: host tests also pass under AddressSanitizer and
UndefinedBehaviorSanitizer, and fail when bugs are planted in copies of the
code. The check add-in matched assembly and C results in the emulator, and
failed with bugs planted in the assembly. Over the 600-state run, colour and
depth of every strip hashed identically in the calculator build and the host
build. The benchmark add-in, frame-logging emulator patch and analysis scripts
were temporary and are not in the repository.

Not done: CPU overclocking (gint's `clock_set_speed`) is not used, because it
affects battery life and hardware margins. If real hardware is too slow, that
is one option; others are writing video RAM through the cache, keeping triangle
setup between strips, or returning to a lower resolution.

## Menus and high score

`menu.c` holds the key handling for play and for the pause and game over menus,
`save.c` the 12-byte checked record, and `cg_main.c` draws the menus. Files are
read and written through `gint_world_switch`, as gint requires. The game reads
keys with `pollevent()`, which does not act on MENU by itself, so `cg_main.c`
calls `gint_osmenu()` and comes back paused. `gint_setrestart(1)` lets the
add-in be reopened after quitting. The 4 and 6 keys no longer steer; the arrows
do.

Host tests cover the key handling and the record, including damaged and short
files. In the emulator the score was saved, loaded in a later run and shown;
MENU returned to the pause menu; and read-only storage produced the failure
message. The emulator has no real main menu and returns from it at once, so
**the trip to the calculator's menu and back, reopening after Quit, and writing
to real storage memory are untested on hardware.**

## Current state

- Imported original meshes are transformed into fixed-point coordinates by the
  converter. The renderer draws at the full 396×224. Its closer camera retains
  the reference's downward pitch while making the runner easier to see.
- Short horizontal dashes appear where the floor meets the planted edging. The
  192×108 build shows them too; the cause has not been investigated.
- Left/right move sideways on straight paths and request a turn near a corner;
  up jumps, down slides, F1 pauses, F6 shows the frame rate, EXIT quits and
  MENU opens the calculator's menu.
- Straight, corner, gap and narrow-path ground polygons come from the temple
  prefabs. Ground queries run after movement and support jumping gaps and falling.
  Missed corners still cause immediate death rather than a falling animation,
  with only the runner left in view.
- The runner uses its bind pose with an approximate slide posture. Animation,
  scenery, original UI, audio, saves other than the high score, power-ups, and
  other game features remain unfinished.

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
   scenes, check input timing, run `TRCHECK.g3a`, press MENU and return, and
   check that the high score survives quitting.
2. Import and bake the actual runner skeleton and animation clips. The runner
   prefab references the uncompressed `Assets/AnimationClip/Run_0.anim` by GUID
   `5258da0604b9bea4fa9a3a3a04473074`. Its mesh has bone weights in vertex stream
   2. Both the converter (position and UV) and `tr_mesh_build` (position only)
   merge vertices, so preserve skin weights in both.
3. Verify ground, turns and collisions across headings and seeds; port falling
   and death presentation, and improve camera tracking during jumps and turns.
4. Add original UI, environment, effects, pickups and the other saved
   progress while keeping source provenance and authored approximations clear.

The reference checkout used to generate the headers in `native/build/` was at
commit `65964ad`; its temporary copy in `/tmp` is gone, so re-clone it before
running the converter again. The separate emulator was `/tmp/temple-emulator`,
also temporary. The cross-build used Docker image `manawyrm/fxsdk`, digest
`sha256:b5283be1ba98d90aadc2cbc767d0424baa801344c71108c587e922b37392beaf`.
