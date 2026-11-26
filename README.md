# Temple Run for Casio fx-CG50

This project is an incomplete native C port of the
[Temple Run Unity reconstruction](https://github.com/LuciusTran/templerun),
targeting an fx-CG50 add-in. It translates selected recovered gameplay and
renders meshes imported from the reconstruction; it does not run Unity itself.

The renderer draws at the screen's full 396×224 resolution, using integer
arithmetic and a few hand-written SH-4 assembly routines. In a scripted gameplay
run in an fx-CG50 emulator it showed about 30 frames a second. The emulator
estimates timing and does not model the processor's caches, so **speed on a
physical calculator is unknown**; press F6 in the game to show the frame rate.

F1 opens a pause menu, a game over menu follows each run, and the high score is
saved on the calculator. Scripted emulator runs cleared the opening tree and
the first junction in both directions. Animation, the original UI, audio and
the rest of the game are unfinished, and calculator compatibility has not been
verified.

## Faster, lower-resolution version

Commit `665c74e`, "Rewrite the renderer for a smoother frame rate", has the
performance boost at the lower resolution: it draws at 192×108 and scales the
picture 2× to the screen. Use it if speed matters more than sharpness. It
predates the menus and the saved high score.

| Scripted gameplay in the emulator | `665c74e`, 192×108 | Full resolution, 396×224 |
| --- | --- | --- |
| Average frames per second | 57.4 | 30.3 |
| Median / worst frame time | 16.9 / 23.3 ms | 31.8 / 44.6 ms |

Before that commit, the same run averaged 12.9 frames a second at 192×108.

To build it, first commit or stash any local changes. Generate the asset headers
if you have not already (see [native/README.md](native/README.md)), then run
this from the project root:

```sh
git switch --detach 665c74e
(cd native && docker run --platform linux/amd64 --network none --rm \
  -v "$PWD:/work" -w /work manawyrm/fxsdk fxsdk build-cg)
git switch -
```

This replaces `native/TRUN3D.g3a` with the low-resolution build, which stays in
place after switching back until you build again.

See [native/README.md](native/README.md) for controls, provenance, build and test
instructions, and [HANDOFF.md](HANDOFF.md) for the current state and next steps.

The reconstruction's scripts and bundled dependencies have not been fully audited.
Original game assets and locally generated add-ins are not tracked here; obtain
the reference assets separately and keep generated outputs local.
