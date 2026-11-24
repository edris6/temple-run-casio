# Temple Run for Casio fx-CG50

This project is an incomplete native C port of the
[Temple Run Unity reconstruction](https://github.com/LuciusTran/templerun),
targeting an fx-CG50 add-in. It translates selected recovered gameplay and
renders meshes imported from the reconstruction; it does not run Unity itself.

Scripted emulator runs cleared the opening tree, first left turn and a test gap.
The renderer uses a lower internal resolution to improve speed. Animation, UI,
audio and saves are unfinished; physical-calculator performance and compatibility
have not been verified.

See [native/README.md](native/README.md) for controls, provenance, build and test
instructions, and [HANDOFF.md](HANDOFF.md) for the current state and next steps.

The reconstruction's scripts and bundled dependencies have not been fully audited.
Original game assets and locally generated add-ins are not tracked here; obtain
the reference assets separately and keep generated outputs local.
