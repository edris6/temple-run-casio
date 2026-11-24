# Temple Run fx-CG50 port: handoff

The goal is a native port of the reconstructed Unity game, not the earlier
Python or Casio BASIC prototypes. The port is incomplete; see
[native/README.md](native/README.md) for controls, build instructions and source
provenance.

## Current state

- Imported original meshes are deduplicated and transformed into fixed-point
  coordinates. The renderer uses a 128×72 internal framebuffer, scaled 3× to
  the screen, and an approximate camera based on `TempleRunFlow.SnapCamera`.
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
- In the same emulator setup, an instrumented two-second opening displayed
  roughly 30 frames after the renderer changes, versus roughly 7 before. This
  is a limited emulator comparison, not a physical-device FPS measurement.
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
