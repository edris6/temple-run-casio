# Real-time fx-CG50 prototype

`TRUN.g3m` is a Casio BASIC program generated from `TRUN.cbas`. It is an
experimental text-screen runner: the game advances automatically, with one
obstacle or coin approaching at a time. It is an original reimagining, not a
conversion of the Unity game's code or artwork.

## Controls and display

- Left and right arrows move one lane per press.
- Up jumps; down slides. The action lasts three movement steps, so use it as
  the obstacle approaches. You can instead move to another lane.
- F6 ends the run.
- `C` is a coin; `G` is a gap, `L` is a log and `B` is a branch. Jump a gap
  or log, and slide under a branch. `R` marks the runner, changing to `J` or
  `S` during a jump or slide.

Passing an object earns 10 points; collecting a coin adds 25. Colliding ends
the run. There is no persistent score or restart menu.

The timing is based on 40 input polls per movement step, not a clock. Actual
speed and responsiveness depend on the calculator and have not been measured.
To change it, edit `T≥40` in `TRUN.cbas`: a larger number makes the game slower.

## Transfer when your cable arrives

1. Back up data you care about. Running this program changes calculator
   numeric variables A, B, C, D, G, H, J, K, N, P, S and T.
2. Connect the fx-CG50 using a USB **data** cable and select USB Flash.
3. Copy `TRUN.g3m` to the calculator's `@MainMem/PROGRAM` folder, then safely
   eject the calculator so it reloads its memory. If that folder is unavailable,
   copy the file to storage memory and use Memory Manager to copy it to main
   memory.
4. Open the PROGRAM menu and run `TRUN`.

The converter's author describes the direct transfer path in the
[CASIO BASIC extension instructions](https://marketplace.visualstudio.com/items?itemName=KurtBrice.casio-basic).
The transfer procedure, generated file, display and gameplay have not yet been
verified on an fx-CG50. Keep the Python prototype as a fallback.

## Source and verification

The `.cbas` is readable Casio BASIC source, using the `→`, `≠`, and `≥`
calculator symbols. It is not a file the calculator can run directly. The
`.g3m` was built with version 0.3.0 of the CASIO BASIC extension's command-line
encoder, decoded and re-encoded byte-for-byte. That checks serialization
consistency, not compatibility with a real device. The extension's own notes
say that running a saved file on a calculator has not yet been tested.

On a computer, `python3 -m unittest discover -s tests -v` runs gameplay
scenarios through a small test interpreter for the BASIC statements in this
program. This is not a calculator emulator and does not test timing, keyboard
hardware, rendering, or `.g3m` execution. The original commands are described
in chapter 8 of [Casio's fx-CG50 manual](https://www.casio.com/content/dam/casio/global/support/manuals/calculators/pdf/004-en/f/fx-CG50_Soft_v360_EN.pdf).
