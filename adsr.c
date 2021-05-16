/*!
 * Polyphonic synthesizer for microcontrollers.  ADSR Envelope generator.
 * (C) 2017 Stuart Longland
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include "debug.h"
#include "adsr.h"
#include <stdlib.h>
#ifdef __AVR_ARCH__
#include <avr/pgmspace.h>
#endif

/* ADSR attack/decay adjustments */
#define ADSR_LIN_AMP_FACTOR	(5)

/*!
 * Configure the ADSR.
 */
void adsr_config(struct adsr_env_gen_t* const adsr, struct adsr_env_def_t* const def) {
	adsr->def = *def;
	adsr_reset(adsr);
}

/*!
 * ADSR Attack amplitude exponential shift.
 */
static uint8_t adsr_attack_amp(uint8_t amp, uint8_t count) {
	if (count >= 8)
		return 0;

	amp >>= count+1;
	return amp;
}

/*!
 * ADSR Release amplitude exponential shift.
 */
static uint8_t adsr_release_amp(uint8_t amp, uint8_t count) {
	return adsr_attack_amp(amp, 16 - count);
}

/*!
 * Compute the ADSR gain
 */
uint8_t adsr_next(struct adsr_env_gen_t* const adsr) {
	if (adsr->next_event) {
		/* Still waiting for next event */
		//if (adsr->next_event != TIME_SCALE_MAX)
			adsr->next_event--;
		_DPRINTF("adsr=%p gain=%d next_in=%d\n",
				adsr, adsr->gain, adsr->next_event);
		return adsr->gain;
	}
	
	/*
	 * We use if statements here since we might want to jump
	 * between states.  This lets us do that more easily.
	 */
	if (adsr->state_counter < ADSR_STATE_DECAY_START) {
		// _DPRINTF("adsr=%p IDLE time_scale=%d "
		// 		"delay_time=%d "
		// 		"attack_time=%d "
		// 		"decay_time=%d "
		// 		"sustain_time=%d "
		// 		"release_time=%d "
		// 		"peak_amp=%d "
		// 		"sustain_amp=%d\n",
		// 		adsr, adsr->time_scale,
		// 		adsr->delay_time,
		// 		adsr->attack_time,
		// 		adsr->decay_time,
		// 		adsr->sustain_time,
		// 		adsr->release_time,
		// 		adsr->peak_amp,
		// 		adsr->sustain_amp);

		/* Are registers set up? */
//		if (!adsr->def.time_scale)
//			return 0;
//		_DPRINTF("adsr=%p time scale set\n", adsr);

        /*
		if (!(
#ifdef ADSR_FIXED_DELAY
                ADSR_FIXED_DELAY
#else
                adsr->def.delay_time
#endif
                || 
#ifdef ADSR_FIXED_ATTACK
                ADSR_FIXED_ATTACK
#else
                adsr->def.attack_time
#endif
				|| 
#ifdef ADSR_FIXED_DECAY
                ADSR_FIXED_DECAY
#else
                adsr->def.decay_time
#endif
				|| adsr->def.sustain_time
				|| adsr->def.release_time))
			return 0;
*/
//		_DPRINTF("adsr=%p envelope timings set\n", adsr);
        /*
		if (!(
#ifdef ADSR_FIXED_PEAK_AMP
                ADSR_FIXED_PEAK_AMP
#else
                adsr->def.peak_amp
#endif
                || 
#ifdef ADSR_FIXED_SUSTAIN_AMP
                ADSR_FIXED_SUSTAIN_AMP
#else
                adsr->def.sustain_amp
#endif
                ))
			return 0;
*/
//		_DPRINTF("adsr=%p envelope amplitudes set\n", adsr);
		/* All good */
/*		if (
#ifdef ADSR_FIXED_DELAY
                ADSR_FIXED_DELAY
#else
                adsr->def.delay_time
#endif
           )
			adsr->state = ADSR_STATE_DELAY_INIT;
		else
*/
			//adsr->state = ADSR_STATE_DELAY_EXPIRE;
	//}
        /*
	if (adsr->state == ADSR_STATE_DELAY_INIT) {
		_DPRINTF("adsr=%p DELAY INIT\n", adsr);

		// Setting up a delay
		adsr->amplitude = 0;
		adsr->next_event = adsr->def.time_scale * 
#ifdef ADSR_FIXED_DELAY
                ADSR_FIXED_DELAY
#else
                adsr->def.delay_time
#endif
                ;
		adsr->state = ADSR_STATE_DELAY_EXPIRE;
		// Wait for delay 
		return adsr->amplitude;
	}
         */

	//if (adsr->state == ADSR_STATE_DELAY_EXPIRE) {
		//_DPRINTF("adsr=%p DELAY EXPIRE\n", adsr);

		/* Delay has expired */
// 		if (
// #ifdef ADSR_FIXED_ATTACK
//                 ADSR_FIXED_ATTACK
// #else
//                 adsr->def.attack_time
// #endif
//            )
// 			adsr->state = ADSR_STATE_ATTACK_INIT;
// 		else
// 			adsr->state = ADSR_STATE_ATTACK_EXPIRE;
// 	}

// 	if (adsr->state == ADSR_STATE_ATTACK_INIT) {
		/* Attack is divided into 16 steps */
// 		adsr->time_step = (TIME_SCALE_T)((
// #ifdef ADSR_FIXED_ATTACK
//                 ADSR_FIXED_ATTACK
// #else
//                 adsr->def.attack_time
// #endif
// 				* adsr->def.time_scale) >> 4);
//		adsr->counter = 16;
//		adsr->state = ADSR_STATE_ATTACK;

		// _DPRINTF("adsr=%p ATTACK INIT tstep=%d\n",
		// 		adsr, adsr->time_step);
	// }

	// if (adsr->state == ADSR_STATE_ATTACK) {
		// _DPRINTF("adsr=%p ATTACK count=%d\n", adsr,
		// 		adsr->counter);

//		if (adsr->counter) {
			/* Change of amplitude */
// 			uint16_t lin_amp = (adsr->state_counter) * 
// #ifdef ADSR_FIXED_PEAK_AMP
//                 ADSR_FIXED_PEAK_AMP
// #else
//                 adsr->def.peak_amp
// #endif
//                     ;
// 			uint16_t exp_amp = adsr_attack_amp(
// #ifdef ADSR_FIXED_PEAK_AMP
//                 ADSR_FIXED_PEAK_AMP
// #else
//                 adsr->def.peak_amp
// #endif
//                     , 16 - adsr->state_counter);
// 			lin_amp >>= ADSR_LIN_AMP_FACTOR;
// 			_DPRINTF("adsr=%p ATTACK lin=%d exp=%d\n",
// 					adsr, lin_amp, exp_amp);
			// from gain 7 to 0
			adsr->gain = 7 - (adsr->state_counter >> 1);
			/* Go around again */
			// adsr->counter--;
			// adsr->next_event = adsr->time_step;
		// 	return adsr->amplitude;
		// } else {
		// 	adsr->state = ADSR_STATE_ATTACK_EXPIRE;
		// }
	}

	else if (adsr->state_counter < ADSR_STATE_SUSTAIN_START) {
//		_DPRINTF("adsr=%p ATTACK EXPIRE\n", adsr);

// 		if (
// #ifdef ADSR_FIXED_DECAY
//                 ADSR_FIXED_DECAY
// #else
//                 adsr->def.decay_time
// #endif
//            )
// 			adsr->state = ADSR_STATE_DECAY_INIT;
// 		else
// 			adsr->state = ADSR_STATE_DECAY_EXPIRE;
// 	}

// 	if (adsr->state == ADSR_STATE_DECAY_INIT) {
// 		_DPRINTF("adsr=%p DECAY INIT\n", adsr);

		/* We should be at full amplitude */
// 		adsr->amplitude = 
// #ifdef ADSR_FIXED_PEAK_AMP
//                 ADSR_FIXED_PEAK_AMP
// #else
//                 adsr->def.peak_amp
// #endif
//                 ;
// 		adsr->time_step = (TIME_SCALE_T)((
// #ifdef ADSR_FIXED_DECAY
//                 ADSR_FIXED_DECAY
// #else
//                 adsr->def.decay_time
// #endif
// 					* adsr->def.time_scale) >> 4);
//		adsr->counter = 16;
//		adsr->next_event = adsr->time_step;
//		adsr->state = ADSR_STATE_DECAY;
//	}

//	if (adsr->state == ADSR_STATE_DECAY) {
//		_DPRINTF("adsr=%p DECAY\n", adsr);

//		if (adsr->counter) {
			/* Linear decrease in amplitude */
// 			uint16_t delta = 
// #ifdef ADSR_FIXED_PEAK_AMP
//                 ADSR_FIXED_PEAK_AMP
// #else
//                 adsr->def.peak_amp
// #endif
//                 - 
// #ifdef ADSR_FIXED_SUSTAIN_AMP
//                 ADSR_FIXED_SUSTAIN_AMP
// #else
//                 adsr->def.sustain_amp
// #endif
//                 ;
// 			delta *= (16 - (adsr->state_counter - ADSR_STATE_DECAY_START));
// 			delta >>= 4;

// 			adsr->amplitude = 
// #ifdef ADSR_FIXED_SUSTAIN_AMP
//                 ADSR_FIXED_SUSTAIN_AMP
// #else
//                 adsr->def.sustain_amp
// #endif
//                     + delta;

		// from gain 0 to 1 (sustain)
		adsr->gain = (adsr->state_counter - ADSR_STATE_DECAY_START) >> 3;

		// 	adsr->next_event = adsr->time_step;
		// 	adsr->counter--;
		// } else {
		// 	adsr->state = ADSR_STATE_DECAY_EXPIRE;
		// }
	// }

	// if (adsr->state == ADSR_STATE_DECAY_EXPIRE) {
	// 	_DPRINTF("adsr=%p DECAY EXPIRE\n", adsr);

	// 	if (adsr->def.sustain_time)
	// 		adsr->state = ADSR_STATE_SUSTAIN_INIT;
	// 	else
	// 		adsr->state = ADSR_STATE_SUSTAIN_EXPIRE;
	// }

	} 
	
	else if (adsr->state_counter < adsr->def.release_start) {
//		_DPRINTF("adsr=%p SUSTAIN INIT\n", adsr);

		adsr->gain = 2;
		// adsr->next_event = adsr->def.time_scale * adsr->def.sustain_time;
		// adsr->state = ADSR_STATE_SUSTAIN_EXPIRE;
		// /* Wait for delay */
		// return adsr->amplitude;
	}

	// else if (adsr->state == ADSR_STATE_SUSTAIN_EXPIRE) {
	// 	_DPRINTF("adsr=%p SUSTAIN EXPIRE\n", adsr);

	// 	if (adsr->def.release_time)
	// 		adsr->state = ADSR_STATE_RELEASE_INIT;
	// 	else
	// 		adsr->state = ADSR_STATE_RELEASE_EXPIRE;
	// }

	// else if (adsr->state == ADSR_STATE_RELEASE_INIT) {
	// 	_DPRINTF("adsr=%p RELEASE INIT\n", adsr);

	// 	adsr->time_step = (TIME_SCALE_T)((adsr->def.release_time
	// 				* adsr->def.time_scale) >> 4);
	// 	adsr->counter = 16;
	// 	adsr->next_event = adsr->time_step;
	// 	adsr->state = ADSR_STATE_RELEASE;
	// }

	else if (adsr->state_counter < adsr->def.release_start + ADSR_STATE_RELEASE_DURATION) {
// //		_DPRINTF("adsr=%p RELEASE\n", adsr);
// 		uint8_t counter = 16 - (adsr->state_counter - adsr->def.release_start);
// //		if (adsr->counter) {
// 			/* Change of amplitude */
// 			uint16_t lin_amp = counter * 
// #ifdef ADSR_FIXED_SUSTAIN_AMP
//                 ADSR_FIXED_SUSTAIN_AMP
// #else
//                 adsr->def.sustain_amp
// #endif
//                        ;
// 			uint16_t exp_amp = adsr_release_amp(
// #ifdef ADSR_FIXED_SUSTAIN_AMP
//                 ADSR_FIXED_SUSTAIN_AMP
// #else
//                 adsr->def.sustain_amp
// #endif
//                     , counter);
// 			lin_amp >>= ADSR_LIN_AMP_FACTOR;
// 			_DPRINTF("adsr=%p RELEASE lin=%d exp=%d\n",
// 					adsr, lin_amp, exp_amp);
// 			adsr->amplitude = lin_amp + exp_amp;

		// From 2 to 5
		adsr->gain = ((adsr->state_counter - adsr->def.release_start) >> 2) + 2;

		// 	/* Go around again */
		// 	adsr->counter--;
		// 	adsr->next_event = adsr->time_step;
		// } else {
		// 	adsr->state = ADSR_STATE_RELEASE_EXPIRE;
		// }
	}

	else {
		_DPRINTF("adsr=%p RELEASE EXPIRE\n", adsr);

		/* Reset the state, mute channel */
		//adsr->state_counter = ADSR_STATE_DONE - 1;
		adsr->gain = 8;
	}

	adsr->next_event = adsr->def.time_scale;
	adsr->state_counter++;
	return adsr->gain;
}

/*
 * vim: set sw=8 ts=8 noet si tw=72
 */
