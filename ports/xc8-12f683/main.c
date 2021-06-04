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

static const uint8_t* tune_ptr;
static const uint8_t* tune_ptr_end;
static uint8_t tune_ptr_bits;

// Return it unmasked
static uint8_t read_bits(uint8_t bits) {
    uint16_t buffer = *tune_ptr + (*(tune_ptr + 1) << 8);
    buffer >>= tune_ptr_bits;
    tune_ptr_bits += bits;
    if (tune_ptr_bits >= 8) {
        tune_ptr_bits -= 8;
        tune_ptr++;
    }
    return buffer;
}

// Slow
void new_frame_require() {
#if BITS_ADSR_TIME_SCALE > 0
	uint8_t ref_adsr_time_scale = read_bits(BITS_ADSR_TIME_SCALE) & ((1 << BITS_ADSR_TIME_SCALE) - 1);
#endif
#if BITS_WF_PERIOD > 0
	uint8_t ref_wf_period = read_bits(BITS_WF_PERIOD) & ((1 << BITS_WF_PERIOD) - 1);
#endif
#if BITS_WF_AMPLITUDE > 0
	uint8_t ref_wf_amplitude = read_bits(BITS_WF_AMPLITUDE) & ((1 << BITS_WF_AMPLITUDE) - 1);
#endif
#if BITS_ADSR_RELEASE_START > 0
	uint8_t ref_adsr_release_start = read_bits(BITS_ADSR_RELEASE_START) & ((1 << BITS_ADSR_RELEASE_START) - 1);
#endif

	if (tune_ptr >= tune_ptr_end
#if BITS_ADSR_TIME_SCALE > 0
            && !ref_adsr_time_scale
#endif
#if BITS_WF_PERIOD > 0
            && !ref_wf_period 
#endif
#if BITS_WF_AMPLITUDE > 0
            && !ref_wf_amplitude 
#endif
#if BITS_ADSR_RELEASE_START > 0
            && !ref_adsr_release_start
#endif
            ) {
		seq_buf_frame.adsr_time_scale_1 = 0;
	} else {
#if BITS_ADSR_TIME_SCALE > 0
		seq_buf_frame.adsr_time_scale_1 = tune_adsr_time_scale_refs[ref_adsr_time_scale];
#else
		seq_buf_frame.adsr_time_scale_1 = tune_adsr_time_scale_refs[0];
#endif
#if BITS_WF_PERIOD > 0
		seq_buf_frame.wf_period = tune_wf_period_refs[ref_wf_period];
#else
		seq_buf_frame.wf_period = tune_wf_period_refs[0];
#endif
#if BITS_WF_AMPLITUDE > 0
		seq_buf_frame.wf_amplitude = tune_wf_amplitude_refs[ref_wf_amplitude];
#else
		seq_buf_frame.wf_amplitude = tune_wf_amplitude_refs[0];
#endif
#if BITS_ADSR_RELEASE_START > 0
		seq_buf_frame.adsr_release_start = tune_adsr_release_start_refs[ref_adsr_release_start];
#else
		seq_buf_frame.adsr_release_start = tune_adsr_release_start_refs[0];
#endif
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
        tune_ptr_end = tune_data + TUNE_DATA_SIZE - 1;
        tune_ptr_bits = 0;
        
        seq_play_stream(SEQ_CHANNEL_COUNT);
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

