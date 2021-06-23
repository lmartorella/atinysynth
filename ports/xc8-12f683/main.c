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
    uint16_t buffer = *tune_ptr + (uint16_t)(*(tune_ptr + 1) << 8);
    buffer >>= tune_ptr_bits;
    tune_ptr_bits += bits;
    if (tune_ptr_bits >= 8) {
        tune_ptr_bits -= 8;
        tune_ptr++;
    }
    return (uint8_t)buffer;
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

	if (tune_ptr >= (tune_ptr_end - 1)
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

void main() {
    while (1) {
        // Set HS 20Mhz
        OSCCONbits.SCS = 0;

        // Set-up PWM
        // Timer prescaler to 1 and PR2 = 0x19 = 76kHZ and 6 bits of resolution @8MHz

        // Disable output
        TRISIObits.TRISIO2 = 1;

        // Timer 2
        T2CONbits.T2CKPS = 0; // 1:1 prescaler
        T2CONbits.TMR2ON = 1; // ON
        // Period = 20Mhz / (4 * (PR2 + 1)) / prescaler = 19.53kHZ
        // Duty cycle = CCP / (4 * (PR2 + 1)). Max is 1024 (10-bit)
        // Very close to hearing range, but this allow the upper 8-bit MSB to
        // be pushed in CCPR1L without bit shifts. The speakers will cut it.
        PR2 = 0xff; 

        // PWM, active high
        CCP1CONbits.CCP1M = 0xC;

        // Setup Timer0 for FREQ (20MHz -> 5MHz Fosc/4)
        OPTION_REGbits.PSA = 0; // prescaler 
        OPTION_REGbits.PS = 0;  // prescaler 1:2 => 9766
        OPTION_REGbits.T0CS = 0; // Fosc

        // Enable PWM output
        TRISIObits.TRISIO2 = 0;
        CCP1CONbits.DC1B = 0;

        for (uint8_t count = 3; count; count--) {
            tune_ptr = tune_data;
            tune_ptr_end = tune_data + TUNE_DATA_SIZE - 1;
            tune_ptr_bits = 0;
            seq_end = 0;
            cur_voice = &synth.voice[0];
            for (uint8_t i = 0; i < VOICE_COUNT; i++, cur_voice++) {
                cur_voice->adsr.state_counter = 0;
            }

            seq_play_stream(SEQ_CHANNEL_COUNT);

            while (!seq_end) {

                // From +128 to -128
                CCPR1L = (uint8_t)(seq_feed_synth()) + 128;            

                // Wait for next sampling op
                while (!INTCONbits.T0IF);
                INTCONbits.T0IF = 0;
            }
        }

        // HALT PWM and set 0 to save battery
        CCP1CONbits.CCP1M = 0x0;
        TRISIObits.TRISIO2 = 0;
        GPIObits.GP2 = 0;

        // HALT CPU
        SLEEP();
    }
}

