// CONFIG
#pragma config FOSC = HS        // Oscillator Selection bits
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
#include "tune_gen.h"
    
struct poly_synth_t synth;

static const struct tune_frame_t* tune_ptr;

void new_frame_require() {
    seq_buf_frame.adsr_time_scale = tune_adsr_time_scale_refs[tune_ptr->adsr_time_scale];
    seq_buf_frame.wf_period = tune_wf_period_refs[tune_ptr->wf_period];
    seq_buf_frame.wf_amplitude = tune_wf_amplitude_refs[tune_ptr->wf_amplitude];
    seq_buf_frame.adsr_release_start = tune_adsr_release_start_refs[tune_ptr->adsr_release_start];
    tune_ptr++;
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
    // Set HS 20Mhz
    OSCCONbits.SCS = 0;
    
    // Set-up PWM
    // Timer prescaler to 1 and PR2 = 0x19 = 76kHZ and 6 bits of resolution @8MHz

    // Disable output
    TRISIObits.TRISIO2 = 1;
    
    // Timer 2
    T2CONbits.T2CKPS = 0; // 1:1 prescaler
    T2CONbits.TMR2ON = 1; // ON
    // Period = 20Mhz / (4 * (PR2 + 1)) / prescaler = 156kHZ
    PR2 = 31; 
    // Duty cycle = CCP / (4 * (PR2 + 1)). Max is 128 (7-bit)
    
    // PWM, active high
    CCP1CONbits.CCP1M = 0xC;

    // Setup Timer0 for FREQ (20MHz -> 5MHz Fosc/4)
    OPTION_REGbits.PSA = 0; // prescaler 
    OPTION_REGbits.PS = 1;  // prescaler 1:4 => 9766 / 2Hz
    OPTION_REGbits.T0CS = 0; // Fosc
    
    // Enable PWM output
    TRISIObits.TRISIO2 = 0;
    
    // Now CCPR1L:CCP1CONbits.DC1B is 7-bits 
    //test_freq();

    while (1) {
        tune_ptr = tune_data;
        seq_play_stream(SEQ_VOICE_COUNT);
        seq_feed_synth();

        while (synth.enable) {
            
            // From +64 to -64
            int8_t sample = poly_synth_next() >> 1;
            uint8_t pwm = (uint8_t)(sample + 64);
            
            // TODO do atomically
            CCPR1L = pwm >> 2;
            CCP1CONbits.DC1B = pwm & 0x3;

            seq_feed_synth();
            
            // Wait for next sampling op
            while (!INTCONbits.T0IF);
            INTCONbits.T0IF = 0;
        }
    }
}

