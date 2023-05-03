# ZXPicoIF2ROM
IF2 ROM Cartridge Replacement using a Raspberry Pico. Makes use of the PIO state machine of the Raspberry Pico which removes the need to overclock the PICO.

Based on the original design by Derek Fountain https://github.com/derekfountain/zx-spectrum-pico-rom

![image](./images/prototype.jpg "Prototype")

## PIO

The PIO constantly reads the address in (A0-A13) and writes the data out (D0-D7) from the correct ROM memory location. It uses DMA determine the memory location and to auto feed the PIO with the correct data to send to the Spectrum. I used the example from Rumbledethumps YouTube video (https://www.youtube.com/watch?v=GOEI2OpMncY&t=374s) as the basis and adapted to work on the ZX Spectrum.

To ensure the Pico doesn't write to the data bus when not required the output enable (OE) pin on the 74LS245 chip is controlled by the OR of MREQ, A14, A15 & RD (1 don't send data, 0 send data)

## Schematic

I redeisgned Derek's schematic in order to put the Address & Data GPIOs in order which is required for the PIO code. I also removed the need for the ROMACCESS connected to the PICO, shifting to just rely on the OE pin of the DATA level converter to stop the Data bus being written to when not required.

![image](./images/schematic.png "Schematic")

## PCB

For the PCB design I moved the components to the reverse of the PCB giving more room. I also shifted to using through hole components instead of surface mount due to my soldering skills.

![image](./images/picoif2.png "PCB")
