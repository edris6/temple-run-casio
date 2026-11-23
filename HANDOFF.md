# Temple Run fx-CG50 port: handoff

Development was stopped at the user's request. The original request is a
native port of the reconstructed Unity game, not the earlier Python or Casio
BASIC approximations.

## Latest continuation: unfinished connected turns

The stable opening-course checkpoint is commit `ca025d1`. After that checkpoint,
work began on a bounded, continuously generated course with world coordinates,
left/right turns, early-turn buffering, horizontal paths and all four original
corner meshes. The current source also adds an approximate smoothed camera and
per-instance vertex caching. It stops safely on later hole/narrow terrain,
which is not implemented. Missing a turn currently causes immediate death at
the tile edge; the Unity reconstruction falls off the track instead.

**This continuation is work in progress, not a verified playable build.** A
cross-built intermediate version ran in the emulator, but a scripted first
turn was missed and the runner died. Instrumentation showed roughly 11 rendered
frames over the first 5.5 emulated seconds; the slow renderer and input latency
need further investigation. The vertex cache was added afterward, but has not
been rebuilt for or measured on the calculator emulator. Do not claim the turn
works or that frame rate is fixed.

The latest host C tests passed with AddressSanitizer and UndefinedBehaviorSanitizer,
including seeded course generation, turn buffering, wrong/missed turns, a long
bounded-course simulation, and input/session checks. A desktop preview rendered
with the vertex cache was byte-identical to the preceding preview. These checks
do not establish that turning, rendering, performance or input works on the
calculator. The local `native/TRUN3D.g3a` is an older intermediate build and
does not include the latest vertex-cache changes; rebuild before testing.

Next, rebuild with fxSDK, repeat the scripted first-turn emulator run, measure
frame rate and input timing, and fix any issues before relying on the connected
course. `native/README.md` describes the earlier stable checkpoint and may not
yet reflect this unfinished continuation. The reference asset importer now also
requires the horizontal path/obstacle meshes and the remaining two corner
meshes; generated original assets remain Git-ignored and must stay local.

## Earlier stable checkpoint

- `native/` contains the fx-CG50 C/gint port. The opening straight uses the
  reference reconstruction's seeded course rules, coin positions, obstacle
  polygons and recovered runner movement/scoring routines.
- Runner, path, coins, trees and the first corner use meshes/textures imported
  from the reference reconstruction. The runner is still in its bind pose.
- Jump, slide, tilt via 4/6, coin collection, obstacle death/stumble,
  pause/resume via F1, and EXE restart are wired. That checkpoint deliberately
  stops at the first corner; the subsequent turn implementation is unverified.
- The locally built diagnostic is `native/TRUN3D.g3a`. It contains derived
  original assets, is ignored by Git, and should remain local. The generated
  headers in `native/build/` and build files in `native/build-cg/` are also
  ignored. The source and tests are tracked.

## Earlier checkpoint verification

- `make -C native test` passed with AddressSanitizer and UndefinedBehaviorSanitizer,
  including a regression for a jump pressed during a slow-render frame.
- `python3 -m unittest discover -s tests -v` passed 14 tests, including asset
  converter checks and the earlier prototypes' tests.
- The native add-in cross-compiled with fxSDK/gint. A scripted headless
  fx-CG50 add-in emulator run jumped the opening tree, collected coins, and
  reached the first corner. Another run showed collision death and verified
  that two paused frames taken two seconds apart were identical.
- Neither a physical calculator nor its timing, memory and input behavior has
  been tested. The emulator result does not establish hardware compatibility.

## Remaining work after the stable checkpoint

1. Extend the track scheduler past the first 12 tiles and port turn buffering,
   connected corners, heading/camera changes, ground regions and missed turns
   from `TempleRunFlow.Track.cs` and `PathElement.cs`.
2. Import the original skeleton and animation clips; implement skinning so the
   runner does not remain in a bind pose. Continue material/lightmap work.
3. Add later obstacles, gaps, power-ups, original UI flow, audio and saves,
   retaining clear source provenance and documenting authored approximations.
4. Re-run emulator checks and profile/test on the actual fx-CG50 once connected.

See `native/README.md` for build instructions, exact source mappings, known
limitations and the emulator script locations. The sparse reference checkout
used during development was `/tmp/templerun-unity-reference` at commit
`65964ad`; `/tmp` is temporary and may be cleared. The separate emulator source
was `/tmp/temple-emulator`. Neither checkout is part of this project.

The `manawyrm/fxsdk` Docker image was downloaded for the cross-build and may
still be present in Docker's image cache. No remote push was performed.

After stopping work, no emulator or build processes were found and Docker
Desktop's daemon was stopped. A root-owned, launchd-managed Docker networking
helper (`com.docker.vmnetd`) remained running; it was not terminated because
it is a system service, and its ownership by this task could not be established.
