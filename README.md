# ZXPicoIF2ROM
IF2 ROM Cartridge Replacement using a Raspberry Pico

## PiO
Makes use of the PIO state machine of the Raspberry Pico.

It constantly reads the address (A0-A13) and writes the data out from the correct ROM location. The output enable is controlled by the 74LS245 chip rather than within the PIO code.

