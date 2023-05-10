#define PROG_NAME   "ZX PicoIF2"
#define VERSION_NUM "v0.5a"

//
// v0.2 initial release
// v0.3 added multi-function to user button, <1second press just reset, >=1 second change the ROM
//      added button bounce protection
// v0.4 use of compressed ROMs
// v0.5 added a ROM selector screen and cycle through ROMs if user button held down
// v0.5a changed USER pin to match v1.1 PCB

// ---------------------------------------------------------------------------
// includes
// ---------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "picoif2.pio.h"
#include "hardware/dma.h"
#include "roms.h"   // the ROMs
//
// ---------------------------------------------------------------------------
// gpio pins
// ---------------------------------------------------------------------------
#define PIN_A0      0   // GPIO 0-13 for A0-A13
#define PIN_D0      14  // GPIO 15-21 for D0-D7
//
#define PIN_RESET   28  // GPIO to control RESET of Spectrum 
#define PIN_USER    22  // User input GPIO (v1.1 PCB this is 22)
//
#define MAXROMS     6
//
void dtoBuffer(uint8_t *to,const uint8_t *from);
//
void main() {
    uint i;
    const uint8_t *roms[] = {original
                            ,diagv59
                            ,testrom
                            ,origtest
                            ,_128kram
                            ,lookglass
                            //,agd
                            };
    //                           012345678901234567890123456789012
    const uint8_t *romname[] = {"Original 48k ROM"
                                ,"Retroleum DiagROM v1.59"
                                ,"ZX Spectrum Diagnositcs v0.37"
                                ,"ZX Spectrum Test Cartridge"
                                ,"128k RAM Tester"
                                ,"Looking Glass ROM"
                                //,"AGD v0.1"
                                };
    uint8_t rompos=0;
    uint8_t rom[16384] __attribute__((aligned (16384))); // align on 16384 boundary so can be used with DMA
    // ------------------------
    // set-up user & reset gpio
    // ------------------------
    gpio_init(PIN_RESET);
    gpio_set_dir(PIN_RESET,GPIO_OUT);
    gpio_put(PIN_RESET,true);    // hold in RESET state till ready 
    gpio_init(PIN_USER);
    gpio_set_dir(PIN_USER,GPIO_IN);
    gpio_pull_up(PIN_USER);
    // ***********************************************************************
    // Set-up PIO
    // ***********************************************************************
    PIO pio=pio0; // use pio 0
    uint addr_data_sm=pio_claim_unused_sm(pio,true); // grab an free state machine from PIO 0
    uint addr_data_offset=pio_add_program(pio,&picoif2_program); // get instruction memory offset for the loaded program
    pio_sm_config addr_data_config=picoif2_program_get_default_config(addr_data_offset); // get the default state machine config
    // --------------
    // set-up IN pins
    // --------------
    for(i=PIN_A0;i<PIN_A0+14;i++) {
        pio_gpio_init(pio,i); // initialise all 14 input pins
        gpio_set_dir(i,GPIO_IN);
        gpio_pull_down(i);
    }    
    sm_config_set_in_pins(&addr_data_config,PIN_A0); // set IN pin base
    sm_config_set_in_shift(&addr_data_config,false,true,14); // shift left 14 pins (A13-A0) into ISR, autopush resultant 32bit address to DMA RXF
    // ---------------
    // set-up OUT pins
    // ---------------    
    for(i=PIN_D0;i<PIN_D0+8;i++) {
        pio_gpio_init(pio,i); // initialise all 8 output pins
        gpio_set_dir(i,GPIO_OUT);
    }
    sm_config_set_out_pins(&addr_data_config,PIN_D0,8); // set OUT pin base & number (bits)
    sm_config_set_out_shift(&addr_data_config,true,true,8); // right shift 8 bits of OSR to pins (D0-D7) with autopull on
    pio_sm_set_consecutive_pindirs(pio,addr_data_sm,PIN_D0,8,true); // set all output pins to output, D0-D7
    pio_sm_init(pio,addr_data_sm,addr_data_offset,&addr_data_config); // reset state machine and configure it
    // ***********************************************************************
    // Set-up DMA
    // ***********************************************************************
    int addr_chan=dma_claim_unused_channel(true);
    int data_chan=dma_claim_unused_channel(true);
    //
    // DMA move the requested memory data to PIO for output *DATA*
    dma_channel_config data_dma=dma_channel_get_default_config(data_chan); // start with default configuration structure
    channel_config_set_high_priority(&data_dma,true); // high priority channels are considered first
    // channel_config_set_dreq - Select a transfer request signal in a channel configuration object
    //   pio_get_dreq - Return the DREQ to use for pacing transfers to/from a particular state machine FIFO
    channel_config_set_dreq(&data_dma,pio_get_dreq(pio,addr_data_sm,true)); // transfer data to PIO
    channel_config_set_transfer_data_size(&data_dma,DMA_SIZE_8); // transfer 8bits at a time
    // channel_config_set_chain_to - Set DMA channel chain_to channel in a channel configuration object
    channel_config_set_chain_to(&data_dma,addr_chan); // trigger addr_chan when complete
    // Configure all DMA parameters
    dma_channel_configure(data_chan,                        // dma channel
                            &data_dma,                      // pointer to DMA config structure
                            &pio->txf[addr_data_sm],        // write to -> PIO FIFO
                            rom,                            // read from array, position in array obtained from *ADDRESS* DMA
                            1,                              // 1 transfer
                            false);                         // do not start transfer
    //
    // DMA move address from PIO into the *DATA* DMA config *ADDRESS*
    dma_channel_config addr_dma=dma_channel_get_default_config(addr_chan); // default configuration structure
    channel_config_set_high_priority(&addr_dma,true); // all high priority channels are considered first
    channel_config_set_dreq(&addr_dma,pio_get_dreq(pio,addr_data_sm,false)); // receive data from PIO
    channel_config_set_read_increment(&addr_dma,false); // read from same address
    channel_config_set_chain_to(&addr_dma,data_chan); // trigger data channel when complete
    dma_channel_configure(addr_chan,                                    // dma channel
                            &addr_dma,                                  // pointer to DMA config structure
                            &dma_channel_hw_addr(data_chan)->read_addr, // write to -> Data DMA read address
                            &pio->rxf[addr_data_sm],                    // read from <- PIO FIFO
                            1,                                          // 1 transfer
                            true);                                      // start transfer immediately
    //
    // start PIO state machine
    pio_sm_put(pio,addr_data_sm,((dma_channel_hw_addr(data_chan)->read_addr)>>14)); // put start memory address into PIO shifted 14bits so when added to the 14 gpios it gives the correct memory location
    pio_sm_set_enabled(pio,addr_data_sm,true); // enable state machine
    // copy in start ROM
    dtoBuffer(rom,roms[rompos]);
    gpio_put(PIN_RESET,false);    // release RESET    
    while(true) {
        if(gpio_get(PIN_USER)==false) {    
            gpio_put(PIN_RESET,true); // put Spectrum in RESET state            
            // wait for button release and check held for 1second to switch ROM otherwise just reset
            uint64_t lastPing=time_us_64();
            while((gpio_get(PIN_USER)==false)&&(time_us_64()<lastPing+1000000)) {
                tight_loop_contents(); 
            }
            while(gpio_get(PIN_USER)==false) {
                if(time_us_64()>=lastPing+1000000) {
                    if(++rompos==MAXROMS) rompos=0;
                    dtoBuffer(rom,switchrom);
                    // change name at 0x64
                    uint j,k=strlen(romname[rompos]);
                    if(k>32) k=32;
                    if(k<31) {
                        j=(32-k)/2;
                        for(i=0;i<j;i++) rom[100+i]=0x20;
                    }
                    for(j=0;j<k;i++,j++) {
                        if(romname[rompos][j]>0x80||romname[rompos][j]<0x20) rom[i+100]=0x20;
                        else rom[i+100]=romname[rompos][j];
                    }
                    while(i<32) {
                        rom[100+i]=0x20;
                        i++;
                    }
                    gpio_put(PIN_RESET,false);    // release RESET
                    lastPing=time_us_64();
                    while((gpio_get(PIN_USER)==false)&&(time_us_64()<lastPing+1000000)) {
                        tight_loop_contents(); 
                    } 
                    while(time_us_64()<lastPing+500000) { // ensure minimum of 0.5 second wait
                        tight_loop_contents(); 
                    }
                    gpio_put(PIN_RESET,true); // put Spectrum in RESET state
                    dtoBuffer(rom,roms[rompos]);
                }               
            } 
            busy_wait_us_32(50000); // de-bounce wait before reset, 50ms
            gpio_put(PIN_RESET,false);    // release RESET  
        }
    }
}

// ---------------------------------------------------------------------------
// dtoBuffer - decompress compressed ROM directly into buffer (simple LZ)
// input:
//   to - the buffer
//   from - the compressed storage
// *simple LZ has a simple 256 backwards window and greedy parser but is very
// fast
// ---------------------------------------------------------------------------
void dtoBuffer(uint8_t *to,const uint8_t *from) { 
    uint i=0,j=0,k;
    uint8_t c,o;
    do {
        c=from[j++];
        if(c==128) return;
        else if(c<128) {
            for(k=0;k<c+1;k++) to[i++]=from[j++];
        }
        else {
            o=from[j++]; // offset
            for(k=0;k<(c-126);k++) {
                to[i]=to[i-(o+1)];
                i++;
            }
        }
    } while(true);
}
