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
#define THRESHOLD 100

volatile uint8_t direction = 0;

#define FWD() direction = 0
#define REV() direction = 1

ISR(ADC_vect)
{
	static uint8_t count = 0;
	static uint8_t a = 0, b = 0, q = 0;

	/* Alternate between ch2 and ch3 */
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
	ADMUX = (1 << ADLAR) | (2 << MUX0);
	/* Free-running mode, CK / 64 = 125 kHz
	 * @13 cycles per sample, about 10 kHz sample rate. 100 us/sample  */
	ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | (1 << ADIE) |
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
	DDRB = 0x02;
	PORTB = 0x02;

	init_adc();
	sei();

	while(1) {
		if (direction)
			_delay_ms(250);
		_delay_ms(250);
		PORTB = 0;
		_delay_ms(250);
		PORTB = 2;
	}

	return 0;
}
