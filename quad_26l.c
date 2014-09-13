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

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define Q_CHANGEMUX 0x80
#define Q_CHANNELA 0x40
#define THRESHOLD 0x20

#define FWD() PORTB = (1 << 4)
#define REV() PORTB = (1 << 5)

ISR(ADC_vect)
{
	static uint8_t count = 0;
	static uint8_t a = 0, b = 0, q = 0;

	/* Alternate between ch0 and ch1 */
	if (count & 1) {
		ADMUX ^= (1 << MUX0);
	} else {
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
}

void init_adc(void)
{
	/* Use channel 0, left adjust result */
	ADMUX = (1 << ADLAR) | (0 << MUX0);
	/* Free-running mode, CK / 64 = 125 kHz
	 * @13 cycles per sample, about 10 kHz sample rate. 100 us/sample  */
	ADCSR = (1 << ADEN) | (1 << ADSC) | (1 << ADFR) | (1 << ADIE) |
		(6 << ADPS0);
}

/* This will clobber *all* ADC state and do a single 8-bit read */
uint8_t analog_read(uint8_t channel)
{
	/* Set some clock. Don't care what */
	ADCSR = (6 << ADPS0);
	/* Select the channel we want. VCC ref, right-adjust */
	ADMUX = (channel << MUX0) | (1 << ADLAR);
	ADCSR |= (1 << ADEN) | (1 << ADSC);

	while(ADCSR & (1 << ADSC));

	return ADCH;
}

int main(void)
{
	uint8_t a_thresh, b_thresh;
	uint8_t reading;
	uint8_t a, b, q;

	DDRA = ~0x03;
	PORTA = 0;
	DDRB = (3 << 4);
	PORTB = 0x00;

	init_adc();
	sei();

	while(1);

	return 0;
}
