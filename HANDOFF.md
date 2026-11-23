# Temple Run fx-CG50 port: handoff

Development was stopped at the user's request. Resume from commit `ca025d1`
and this document. The original request is a native port of the reconstructed
Unity game, not the earlier Python or Casio BASIC approximations.

## Current state

- `native/` contains the fx-CG50 C/gint port. The opening straight uses the
  reference reconstruction's seeded course rules, coin positions, obstacle
  polygons and recovered runner movement/scoring routines.
- Runner, path, coins, trees and the first corner use meshes/textures imported
  from the reference reconstruction. The runner is still in its bind pose.
- Jump, slide, tilt via 4/6, coin collection, obstacle death/stumble,
  pause/resume via F1, and EXE restart are wired. The current build deliberately
  stops at the first corner: turning and later track generation are not ported.
- The locally built diagnostic is `native/TRUN3D.g3a`. It contains derived
  original assets, is ignored by Git, and should remain local. The generated
  headers in `native/build/` and build files in `native/build-cg/` are also
  ignored. The source and tests are tracked.

## Verification completed

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

## Next work

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
