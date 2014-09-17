/*
 * Copyright Brian Starkey 2014 <stark3y@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define F_CPU 8000000

#define DEBUG

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "i2c_machine.h"
#include "i2c_slave_defs.h"

#define LED_ON() PORTB |= 0x2
#define LED_OFF() PORTB &= ~0x2
#define LED_FLICKER() LED_OFF(); LED_ON()

#define FWD() REG_CNT++
#define REV() REG_CNT--

#define FLAG_IRQ_FIRED   (1 << 0)
#define FLAG_CAL_STARTED (1 << 1)

void adc_init(void)
{
	/* Use channel 2, left adjust result */
	ADMUX = (1 << ADLAR) | (2 << MUX0);
	/* Free-running mode, CK / 64 = 125 kHz
	 * @13 cycles per sample, about 10 kHz sample rate. 100 us/sample  */
	ADCSRA = (1 << ADEN) | (1 << ADSC) | (0 << ADATE) | (0 << ADIE) |
		(6 << ADPS0);

}

/* This will clobber *all* ADC state and do a single 8-bit read */
uint8_t analog_read(uint8_t channel)
{
	/* Set some clock. Don't care what */
	ADCSRA = (6 << ADPS0);
	/* Select the channel we want. VCC ref, right-adjust */
	ADMUX = (channel << MUX0) | (1 << ADLAR);
	ADCSRA |= (1 << ADEN) | (1 << ADSC);

	while(ADCSRA & (1 << ADSC));

	return ADCH;
}

int main(void)
{
	uint8_t flags = 0;
	uint8_t sample_count = 0;
	uint8_t q = 0;

	DDRB = 0x02;
	PORTB = 0x00;

#ifdef DEBUG
	LED_ON();
	_delay_ms(30);
	LED_OFF();
	_delay_ms(30);
	LED_ON();
	_delay_ms(30);
	LED_OFF();
#endif

	i2c_init();
	adc_init();
	sei();

	while (1) {
		/* Check for end of i2c transaction */
		i2c_check_stop();

		/* Reset if necessary */
		if (REG_STATUS & STATUS_RST) {
			REG_CNT = 0;
			REG_STATUS &= ~STATUS_RST;
		}

		/* Sample ADC */
		if (ADCSRA & (1 << ADIF)) {
			uint8_t sample = ADCH;
			/* Swap channel */
			ADMUX ^= (1 << MUX0);
			/* Start a new sample */
			ADCSRA |= (1 << ADIF) | (1 << ADSC);

			if (REG_STATUS & STATUS_CAL) {
				if (!(flags & FLAG_CAL_STARTED)) {
					/* Starting new calibration */
					REG_MAX = 0x0;
					REG_MIN = 0xFF;
					flags |= FLAG_CAL_STARTED;
				}
				if (sample > REG_MAX) {
					uint8_t diff = sample - REG_MAX;
					diff >>= 1;
					REG_MAX += diff;
				}
				if (sample < REG_MIN) {
					uint8_t diff = REG_MIN - sample;
					diff >>= 1;
					REG_MIN -= diff;
				}
			} else {
				if (flags & FLAG_CAL_STARTED) {
					/* The bit was just cleared */
					uint8_t threshold = REG_MAX - REG_MIN;
					threshold >>= 1;
					REG_THRESH = REG_MIN + threshold;
					flags &= ~FLAG_CAL_STARTED;
				} else {
					/* q contains the current and previous samples in the LS
					 * 4 bits. Let c[n] = a[n] ^ b[n], then q contains:
					 * |   3   |   2   |   1   |   0   |
					 * | b[n-1]| c[n-1]|  b[n] |  c[n] |
					 *
					 * If q == 0xC, then the last value was 11 and the new one
					 * is 00, meaning the counter should be incremented
					 * If q == 0x3, then the last value was 00 and the new one
					 * is 11, meaning the counter should be decremented
					 */
					if (sample_count & 1) {
						/* 'a' sample - shift (a ^ b) into q */
						if (sample > REG_THRESH) {
							sample = q ^ 1;
						} else {
							sample = q;
						}
						sample &= 1;
						q <<= 1;
						q |= sample;

						/* Now we have 'a' and 'b', check for a transition */
						q = q & 0xF;
						if (q == 0xC) {
							REG_CNT++;
						} else if (q == 0x3) {
							REG_CNT--;
						}
					} else {
						/* 'b' sample - just shift it into q */
						q <<= 1;
						if (sample > REG_THRESH) {
							q |= 1;
						}
					}
					sample_count++;
				}
			}

		}

		/* Check/set interrupt */
		if (REG_STATUS & STATUS_CMIE) {
			/* Detect a match and set the flag */
			if ((REG_CNT == REG_CMP) && !(flags & FLAG_IRQ_FIRED)) {
				REG_STATUS |= STATUS_CMIF;
				flags |= FLAG_IRQ_FIRED;
			}

		}
		/* Detect the flag being cleared and re-arm */
		if (!(REG_STATUS & STATUS_CMIF) && (flags & FLAG_IRQ_FIRED)) {
			flags &= ~FLAG_IRQ_FIRED;
		}

		/* Check/Set LED/IRQ */
		switch (REG_STATUS & LED_STATE_MASK) {
		case LED_STATE_BIT0:
			if (REG_CNT & 1)
				LED_ON();
			else
				LED_OFF();
			break;
		case LED_STATE_IRQ:
			if (REG_STATUS & STATUS_CMIF)
				LED_ON();
			else
				LED_OFF();
		}
	};

	return 0;
}
