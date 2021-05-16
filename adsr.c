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
 * Helper macro, returns the time in samples if the number of
 * time units != ADSR_INFINITE (infinite), otherwise it returns
 * TIME_SCALE_MAX.
 */
static inline TIME_SCALE_T adsr_num_samples(TIME_SCALE_T scale, uint8_t units) {
	if (units != ADSR_INFINITE)
		return scale * units;
	else
		return TIME_SCALE_MAX;
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
 * Compute the ADSR amplitude
 */
uint8_t adsr_next(struct adsr_env_gen_t* const adsr) {
	if (adsr->next_event) {
		/* Still waiting for next event */
		if (adsr->next_event != TIME_SCALE_MAX)
			adsr->next_event--;
		_DPRINTF("adsr=%p amp=%d next_in=%d\n",
				adsr, adsr->amplitude, adsr->next_event);
		return adsr->amplitude;
	}

	/*
	 * We use if statements here since we might want to jump
	 * between states.  This lets us do that more easily.
	 */

	if (adsr->state == ADSR_STATE_IDLE) {
		_DPRINTF("adsr=%p IDLE time_scale=%d "
				"delay_time=%d "
				"attack_time=%d "
				"decay_time=%d "
				"sustain_time=%d "
				"release_time=%d "
				"peak_amp=%d "
				"sustain_amp=%d\n",
				adsr, adsr->time_scale,
				adsr->delay_time,
				adsr->attack_time,
				adsr->decay_time,
				adsr->sustain_time,
				adsr->release_time,
				adsr->peak_amp,
				adsr->sustain_amp);

		/* Are registers set up? */
		if (!adsr->def.time_scale)
			return 0;
		_DPRINTF("adsr=%p time scale set\n", adsr);

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
		_DPRINTF("adsr=%p envelope timings set\n", adsr);

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
		_DPRINTF("adsr=%p envelope amplitudes set\n", adsr);

		/* All good */
		if (
#ifdef ADSR_FIXED_DELAY
                ADSR_FIXED_DELAY
#else
                adsr->def.delay_time
#endif
           )
			adsr->state = ADSR_STATE_DELAY_INIT;
		else
			adsr->state = ADSR_STATE_DELAY_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_DELAY_INIT) {
		_DPRINTF("adsr=%p DELAY INIT\n", adsr);

		/* Setting up a delay */
		adsr->amplitude = 0;
		adsr->next_event = adsr_num_samples(
				adsr->def.time_scale, 
#ifdef ADSR_FIXED_DELAY
                ADSR_FIXED_DELAY
#else
                adsr->def.delay_time
#endif
                );
		adsr->state = ADSR_STATE_DELAY_EXPIRE;
		/* Wait for delay */
		return adsr->amplitude;
	}

	if (adsr->state == ADSR_STATE_DELAY_EXPIRE) {
		_DPRINTF("adsr=%p DELAY EXPIRE\n", adsr);

		/* Delay has expired */
		if (
#ifdef ADSR_FIXED_ATTACK
                ADSR_FIXED_ATTACK
#else
                adsr->def.attack_time
#endif
           )
			adsr->state = ADSR_STATE_ATTACK_INIT;
		else
			adsr->state = ADSR_STATE_ATTACK_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_ATTACK_INIT) {
		/* Attack is divided into 16 segments */
		adsr->time_step = (TIME_SCALE_T)((
#ifdef ADSR_FIXED_ATTACK
                ADSR_FIXED_ATTACK
#else
                adsr->def.attack_time
#endif
				* adsr->def.time_scale) >> 4);
		adsr->counter = 16;
		adsr->next_event = adsr->time_step;
		adsr->state = ADSR_STATE_ATTACK;

		_DPRINTF("adsr=%p ATTACK INIT tstep=%d\n",
				adsr, adsr->time_step);
	}

	if (adsr->state == ADSR_STATE_ATTACK) {
		_DPRINTF("adsr=%p ATTACK count=%d\n", adsr,
				adsr->counter);

		if (adsr->counter) {
			/* Change of amplitude */
			uint16_t lin_amp = (16-adsr->counter) * 
#ifdef ADSR_FIXED_PEAK_AMP
                ADSR_FIXED_PEAK_AMP
#else
                adsr->def.peak_amp
#endif
                    ;
			uint16_t exp_amp = adsr_attack_amp(
#ifdef ADSR_FIXED_DELAY
                ADSR_FIXED_PEAK_AMP
#else
                adsr->def.peak_amp
#endif
                    , adsr->counter);
			lin_amp >>= ADSR_LIN_AMP_FACTOR;
			_DPRINTF("adsr=%p ATTACK lin=%d exp=%d\n",
					adsr, lin_amp, exp_amp);
			adsr->amplitude = lin_amp + exp_amp;
			/* Go around again */
			adsr->counter--;
			adsr->next_event = adsr->time_step;
			return adsr->amplitude;
		} else {
			adsr->state = ADSR_STATE_ATTACK_EXPIRE;
		}
	}

	if (adsr->state == ADSR_STATE_ATTACK_EXPIRE) {
		_DPRINTF("adsr=%p ATTACK EXPIRE\n", adsr);

		if (
#ifdef ADSR_FIXED_DECAY
                ADSR_FIXED_DECAY
#else
                adsr->def.decay_time
#endif
           )
			adsr->state = ADSR_STATE_DECAY_INIT;
		else
			adsr->state = ADSR_STATE_DECAY_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_DECAY_INIT) {
		_DPRINTF("adsr=%p DECAY INIT\n", adsr);

		/* We should be at full amplitude */
		adsr->amplitude = 
#ifdef ADSR_FIXED_PEAK_AMP
                ADSR_FIXED_PEAK_AMP
#else
                adsr->def.peak_amp
#endif
                ;
		adsr->time_step = (TIME_SCALE_T)((
#ifdef ADSR_FIXED_DECAY
                ADSR_FIXED_DECAY
#else
                adsr->def.decay_time
#endif
					* adsr->def.time_scale) >> 4);
		adsr->counter = 16;
		adsr->next_event = adsr->time_step;
		adsr->state = ADSR_STATE_DECAY;
	}

	if (adsr->state == ADSR_STATE_DECAY) {
		_DPRINTF("adsr=%p DECAY\n", adsr);

		if (adsr->counter) {
			/* Linear decrease in amplitude */
			uint16_t delta = 
#ifdef ADSR_FIXED_PEAK_AMP
                ADSR_FIXED_PEAK_AMP
#else
                adsr->def.peak_amp
#endif
                - 
#ifdef ADSR_FIXED_SUSTAIN_AMP
                ADSR_FIXED_SUSTAIN_AMP
#else
                adsr->def.sustain_amp
#endif
                ;
			delta *= adsr->counter;
			delta >>= 4;

			adsr->amplitude = 
#ifdef ADSR_FIXED_SUSTAIN_AMP
                ADSR_FIXED_SUSTAIN_AMP
#else
                adsr->def.sustain_amp
#endif
                    + delta;
			adsr->next_event = adsr->time_step;
			adsr->counter--;
		} else {
			adsr->state = ADSR_STATE_DECAY_EXPIRE;
		}
	}

	if (adsr->state == ADSR_STATE_DECAY_EXPIRE) {
		_DPRINTF("adsr=%p DECAY EXPIRE\n", adsr);

		if (adsr->def.sustain_time)
			adsr->state = ADSR_STATE_SUSTAIN_INIT;
		else
			adsr->state = ADSR_STATE_SUSTAIN_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_SUSTAIN_INIT) {
		_DPRINTF("adsr=%p SUSTAIN INIT\n", adsr);

		adsr->amplitude = 
#ifdef ADSR_FIXED_SUSTAIN_AMP
                ADSR_FIXED_SUSTAIN_AMP
#else
                adsr->def.sustain_amp
#endif
                  ;
		adsr->next_event = adsr_num_samples(
				adsr->def.time_scale, adsr->def.sustain_time);
		adsr->state = ADSR_STATE_SUSTAIN_EXPIRE;
		/* Wait for delay */
		return adsr->amplitude;
	}

	if (adsr->state == ADSR_STATE_SUSTAIN_EXPIRE) {
		_DPRINTF("adsr=%p SUSTAIN EXPIRE\n", adsr);

		if (adsr->def.release_time)
			adsr->state = ADSR_STATE_RELEASE_INIT;
		else
			adsr->state = ADSR_STATE_RELEASE_EXPIRE;
	}

	if (adsr->state == ADSR_STATE_RELEASE_INIT) {
		_DPRINTF("adsr=%p RELEASE INIT\n", adsr);

		adsr->time_step = (TIME_SCALE_T)((adsr->def.release_time
					* adsr->def.time_scale) >> 4);
		adsr->counter = 16;
		adsr->next_event = adsr->time_step;
		adsr->state = ADSR_STATE_RELEASE;
	}

	if (adsr->state == ADSR_STATE_RELEASE) {
		_DPRINTF("adsr=%p RELEASE\n", adsr);

		if (adsr->counter) {
			/* Change of amplitude */
			uint16_t lin_amp = adsr->counter * 
#ifdef ADSR_FIXED_SUSTAIN_AMP
                ADSR_FIXED_SUSTAIN_AMP
#else
                adsr->def.sustain_amp
#endif
                       ;
			uint16_t exp_amp = adsr_release_amp(
#ifdef ADSR_FIXED_SUSTAIN_AMP
                ADSR_FIXED_SUSTAIN_AMP
#else
                adsr->def.sustain_amp
#endif
                    , adsr->counter);
			lin_amp >>= ADSR_LIN_AMP_FACTOR;
			_DPRINTF("adsr=%p RELEASE lin=%d exp=%d\n",
					adsr, lin_amp, exp_amp);
			adsr->amplitude = lin_amp + exp_amp;
			/* Go around again */
			adsr->counter--;
			adsr->next_event = adsr->time_step;
		} else {
			adsr->state = ADSR_STATE_RELEASE_EXPIRE;
		}
	}

	if (adsr->state == ADSR_STATE_RELEASE_EXPIRE) {
		_DPRINTF("adsr=%p RELEASE EXPIRE\n", adsr);

		/* Reset the state */
		adsr->state = ADSR_STATE_DONE;
		adsr->amplitude = 0;
	}

	return adsr->amplitude;
}

/*
 * vim: set sw=8 ts=8 noet si tw=72
 */
