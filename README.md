# Temple Runner for Casio fx-CG50

The requested native port is under [native/](native/README.md). It currently
contains translated gameplay routines, Unity mesh/texture converters and a
native renderer diagnostic. It is **not yet a playable port of
Temple Run**: the level, original animations, textures, turns, obstacles,
coins and game flow still need integration. See the native README for exact
source mappings, build instructions and verification status.

## Earlier prototypes

Two small, original runner prototypes inspired by Temple Run. They do not
include assets, code, or binaries from the referenced game repository. Score
values and course generation are new design choices, not recovered game rules.

The newer [real-time Casio BASIC prototype](realtime/README.md) uses the arrow
keys and advances automatically. The Python version below is turn-based and
can also be played on a computer. Neither has been tested on physical hardware.

## Play the Python prototype

Each turn, the next tile is shown across three lanes, with your runner (`R`)
below. `!` marks a hazard, `$` a coin and `-` an empty lane. Enter a number
and press **EXE**:

| Key | Action |
| --- | --- |
| 4 / 6 | Move one lane left / right |
| 8 | Jump over a gap or log in your lane |
| 2 | Slide under a branch in your lane |
| 5 | Run straight |
| 0 | End the run |

You can also move out of a hazard's lane. Each successful tile earns 10
points, and a coin earns another 25. Each turn resolves the visible tile;
there is no timer or reaction-speed requirement.

## Run the Python prototype on the calculator

The fx-CG50 needs Python mode installed. Once you have a suitable USB data
cable, connect the calculator to your computer and use its USB storage mode.
Copy **both** `TEMPLE.py` and `RUNNER.py` to the same folder in calculator
storage, safely eject it, then open the calculator's Python mode, select
`RUNNER.py`, and press RUN. The game imports `TEMPLE.py`, so keep the files
together. For each action enter its digit, then press EXE. On the calculator,
the screen scrolls as the game prints new turns.

Casio describes Python file transfer and external-file limits in its
[fx-CG50 software manual](https://www.casio.com/content/dam/casio/global/support/manuals/calculators/pdf/004-en/f/fx-CG50_Soft_v360_EN.pdf),
chapter 17. Older OS versions may require a software update for Python mode.
Neither transfer nor execution has been tested on physical hardware yet.

To play on a computer with Python 3:

```sh
python3 RUNNER.py
```

To check the game logic on a computer:

```sh
python3 -m unittest discover -s tests -v
```

## Hardware and scope

Casio lists a 384 x 216 color display and USB connection for the fx-CG50; its
Python environment is an adapted MicroPython, with a drawing area of 384 x 192
pixels. Its documented Python input waits for entry, while the built-in
Basic-like language has a `Getkey` polling command. The real-time prototype
uses that command and the text screen. A graphical 3D-style game, corners,
calibrated pacing and saves remain future work.

## Safety and provenance

The referenced [Unity reconstruction](https://github.com/LuciusTran/templerun)
is not checked out here. Its public description and the supplied architecture
notes were reviewed, but its scripts, dependencies and bundled DLLs have not
been audited or run. Its safety cannot be established from a README or project
description. It contains assets described as extracted from the original game,
which should not be copied into this project without appropriate rights. A
proper source and binary review requires an actual checkout at a pinned commit.

Git tracks this project locally. `git log --oneline` shows saved snapshots;
`git status` shows uncommitted edits. No remote is configured.
