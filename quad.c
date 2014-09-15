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

#define THRESHOLD 100

#define FWD() REG_CNT++
#define REV() REG_CNT--

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
	uint8_t irq_fired;

	DDRB = 0x02;
	PORTB = 0x00;

	i2c_init();
	adc_init();
	sei();

	while (1) {
		i2c_check_stop();

		/* Reset if necessary */
		if (REG_STATUS & STATUS_RST) {
			REG_CNT = 0;
			REG_STATUS &= ~STATUS_RST;
		}

		/* Sample ADC */
		if (ADCSRA & (1 << ADIF)) {
			static uint8_t count = 0;
			static uint8_t a = 0, b = 0, q = 0;
			/* Alternate between ch2 and ch3 */
			if (count & 1) {
				ADMUX ^= (1 << MUX0);
			}
			else {
				uint8_t high = ADCH > THRESHOLD;
				uint8_t newq;
				if (count & 2) {
					if (high) {
						a = 1;
					} else {
						a = 0;
					}
				} else {
					if (high) {
						b = 1;
					} else {
						b = 0;
					}
				}
				newq = (b << 1) | (a ^ b);
				if ((newq == 3) && (q == 0))
					FWD();
				else if ((newq == 0) && (q == 3))
					REV();
				q = newq;
			}
			count++;
			ADCSRA |= (1 << ADIF) | (1 << ADSC);
		}

		/* Check/set interrupt */
		if (REG_STATUS & STATUS_CMIE) {
			/* Detect a match and set the flag */
			if ((REG_CNT == REG_CMP) && !irq_fired) {
				REG_STATUS |= STATUS_CMIF;
				irq_fired = 1;
			}

		}
		/* Detect the flag being cleared and re-arm */
		if (!(REG_STATUS & STATUS_CMIF) && irq_fired) {
			irq_fired = 0;
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
