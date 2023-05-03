#define PROG_NAME   "ZX PicoIF2"
#define VERSION_NUM "v0.2"

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
#define PIN_USER    26  // User input GPIO
//
#define MAXROMS     4
//
// ---------------------------------------------------------------------------
// gpio masks
// ---------------------------------------------------------------------------
//                                2         1         0
//                        87654321098765432109876543210
#define MASK_USER         0b100000000000000000000000000 // USER
#define MASK_RESET      0b10000000000000000000000000000 // RESET
//
void main() {
    uint i;
    // ---------------------------
    // copy correct ROM into place
    // ---------------------------
    const uint8_t *roms[] = {retroleum_diag_v59,gosh_wonderful_1_32,original,diag};
    uint8_t rompos=0;
    uint8_t rom[16384] __attribute__((aligned (16384))); // align on 16384 boundary so can be used with DMA
    //
    for(i=0;i<16384;i++) rom[i]=roms[rompos][i];
    // ------------------------
    // set-up user & reset gpio
    // ------------------------
    gpio_init(PIN_USER);
    gpio_set_dir(PIN_USER,GPIO_IN);
    gpio_pull_up(PIN_USER);
    gpio_init(PIN_RESET);
    gpio_set_dir(PIN_RESET,GPIO_OUT);
    gpio_put(PIN_RESET,true);    // hold in RESET state till ready *not really required for PIO as it is fast enough but used for swapping ROMs    
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
    // reset when ready
    gpio_put(PIN_RESET,false);    // release RESET    
    while(true) {
        if((gpio_get_all()&MASK_USER)==0) {  
            gpio_put(PIN_RESET,true);
            if(++rompos==MAXROMS) rompos=0;
            for(i=0;i<16384;i++) rom[i]=roms[rompos][i];
            while((gpio_get_all()&MASK_USER)==0) {
                tight_loop_contents();
            }     
            busy_wait_us_32(5000); // 5ms wait before reset
            gpio_put(PIN_RESET,false);    // release RESET  
        }
    }
}