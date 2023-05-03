# ZXPicoIF2ROM
IF2 ROM Cartridge Replacement using a Raspberry Pico. Makes use of the PIO state machine of the Raspberry Pico.

Based on the original design by Derek Fountain https://github.com/derekfountain/zx-spectrum-pico-rom

![image](./Images/prototype.jpg "Prototype")

## PiO

It constantly reads the address in (A0-A13) and writes the data out (D0-D7) from the correct ROM memory location. To ensure the Pico doesn't write to the data bus when not required the output enable (OE) pin on the 74LS245 chip is controlled by the OR of MREQ, A14, A15 & RD (1 don't send data, 0 send data)

