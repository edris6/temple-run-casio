# Temple Run for Casio fx-CG50

This project is an incomplete native C port of the
[Temple Run Unity reconstruction](https://github.com/LuciusTran/templerun),
targeting an fx-CG50 add-in. It translates selected recovered gameplay and
renders meshes imported from the reconstruction; it does not run Unity itself.

One scripted emulator run cleared the opening tree and first left turn. Rendering
is slow, and gaps, narrow-path collisions, animation, UI, audio and saves are
unfinished. The port has not been tested on a physical calculator.

See [native/README.md](native/README.md) for controls, provenance, build and test
instructions, and [HANDOFF.md](HANDOFF.md) for the current state and next steps.

The reconstruction's scripts and bundled dependencies have not been fully audited.
Original game assets and locally generated add-ins are not tracked here; obtain
the reference assets separately and keep generated outputs local.
