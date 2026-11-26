# Temple Run for Casio fx-CG50

This project is an incomplete native C port of the
[Temple Run Unity reconstruction](https://github.com/LuciusTran/templerun),
targeting an fx-CG50 add-in. It translates selected recovered gameplay and
renders meshes imported from the reconstruction; it does not run Unity itself.

The renderer draws at 192×108, scaled 2× to the screen, using integer
arithmetic and a few hand-written SH-4 assembly loops. In a scripted gameplay
run in an fx-CG50 emulator it showed about 57 frames a second. The emulator
estimates timing and does not model the processor's caches, so **speed on a
physical calculator is unknown**; press F6 in the game to show the frame rate.

Scripted emulator runs cleared the opening tree and the first junction in both
directions. Animation, the original UI, audio and saves are unfinished, and
calculator compatibility has not been verified.

See [native/README.md](native/README.md) for controls, provenance, build and test
instructions, and [HANDOFF.md](HANDOFF.md) for the current state and next steps.

The reconstruction's scripts and bundled dependencies have not been fully audited.
Original game assets and locally generated add-ins are not tracked here; obtain
the reference assets separately and keep generated outputs local.
