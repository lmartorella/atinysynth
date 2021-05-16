// CONFIG
#pragma config FOSC = INTOSCIO  // Oscillator Selection bits (INTOSCIO oscillator: I/O function on RA4/OSC2/CLKOUT pin, I/O function on RA5/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = ON       // Power-up Timer Enable bit (PWRT enabled)
#pragma config MCLRE = ON       // MCLR Pin Function Select bit (MCLR pin function is MCLR)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)
#pragma config BOREN = ON       // Brown Out Detect (BOR enabled)
#pragma config IESO = ON        // Internal External Switchover bit (Internal External Switchover mode is enabled)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enabled bit (Fail-Safe Clock Monitor is enabled)

#include <xc.h>
#include <stdlib.h>
#include <string.h>
#include "synth.h"
#include "sequencer.h"

struct poly_synth_t synth;

#define COUNT 3

static const struct seq_stream_header_t seq_stream_header = { SYNTH_FREQ, sizeof(struct seq_frame_t), 3, COUNT };
static const struct seq_frame_t seq_stream_data[COUNT] = {
    { { 100, 0x50 }, { 127, SYNTH_FREQ * 16 / 440 } },
    { { 100, 0x50 }, { 127, SYNTH_FREQ * 16 / 440 } },
    { { 100, 0x50 }, { 127, SYNTH_FREQ * 16 / 440 } },
};

static uint16_t i = 0;

uint8_t new_frame_require(struct seq_frame_t* frame) {
    if (i < COUNT) {
        memcpy(frame, seq_stream_data + i, sizeof(struct seq_frame_t));
        i++;
        return 1;
    } else {
        return 0;
    }
}

// Creates a SYNTH_FREQ / 4 square wave
static void test_freq() {
    // Wait for next sampling op
    while (1) {
        while (!INTCONbits.T0IF);
        INTCONbits.T0IF = 0;
        while (!INTCONbits.T0IF);
        INTCONbits.T0IF = 0;

        CCPR1L = 16 >> 2;
        CCP1CONbits.DC1B = 0;

        while (!INTCONbits.T0IF);
        INTCONbits.T0IF = 0;
        while (!INTCONbits.T0IF);
        INTCONbits.T0IF = 0;

        CCPR1L = 112 >> 2;
        CCP1CONbits.DC1B = 0;
    }
}

void main() {
    // Set 8Mhz
    OSCCONbits.IRCF = 0x7;
    
    // Set-up PWM
    // Timer prescaler to 1 and PR2 = 0x19 = 76kHZ and 6 bits of resolution @8MHz

    // Disable output
    TRISIObits.TRISIO2 = 1;
    
    // Timer 2
    T2CONbits.T2CKPS = 0; // 1:1 prescaler
    T2CONbits.TMR2ON = 1; // ON
    // Period = 8Mhz / (4 * (PR2 + 1)) / prescaler = 62.5kHZ
    PR2 = 31; 
    // Duty cycle = CCP / (4 * (PR2 + 1)). Max is 128 (7-bit)
    
    // PWM, active high
    CCP1CONbits.CCP1M = 0xC;

    // Setup Timer0 for FREQ
    OPTION_REGbits.PS = 0; // 1:2 => 3906Hz
    OPTION_REGbits.PSA = 0; // prescaler to TMR0
    OPTION_REGbits.T0CS = 0; // Fosc
    
    // Enable PWM output
    TRISIObits.TRISIO2 = 0;
    
    // Now CCPR1L:CCP1CONbits.DC1B is 7-bits 
    //test_freq();

    while (1) {
        seq_play_stream(&seq_stream_header);
        i = 0;

        seq_feed_synth();
        uint8_t level = 1;

        while (synth.enable) {
            
            // From +64 to -64
            int8_t sample = poly_synth_next() >> 1;
            uint8_t pwm = sample + 64;
            
            // TODO do atomically
            CCPR1L = level >> 2;
            CCP1CONbits.DC1B = level & 0x3;

            seq_feed_synth();
            
            // Wait for next sampling op
            while (!INTCONbits.T0IF);
            INTCONbits.T0IF = 0;
            
            level = level == 1 ? 120 : 1;
        }
    }
}

