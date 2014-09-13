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

#define USI_PORT PORTB
#define USI_DDR DDRB
#define SDA 0
#define SCL 2
#define SDA_OUT 1
#define SDA_IN 0
#define SLAVE_ADDR 0x40
#define NAK() USIDR = 0x80
#define ACK() USIDR = 0x00

#define LED_ON() PORTB |= 0x2
#define LED_OFF() PORTB &= ~0x2
#define LED_FLICKER() LED_OFF(); LED_ON()

#define N_REG 8

#define Q_CHANGEMUX 0x80
#define Q_CHANNELA 0x40
#define THRESHOLD 100

#define STATE_ADDR_MATCH   0
#define STATE_REG_ADDR     1
#define STATE_MASTER_READ  2
#define STATE_MASTER_WRITE 3

volatile uint8_t direction = 0;
volatile uint8_t usi_state = 0;
volatile uint8_t registers[N_REG] = { 1, 2, 3, 4, 5, 6, 7, 8 };
uint8_t reg_offset = 0;

#define FWD() direction = 0
#define REV() direction = 1

/* USI i2c Slave State Machine
 * ===========================
 *
 * 4 States:
 *     0 STATE_ADDR_MATCH
 *       Waiting for address (idle)
 *
 *     1 STATE_REG_ADDR
 *       Receive register address*
 *
 *     2 STATE_MASTER_READ
 *       Transmit data to master
 *
 *     3 STATE_MASTER_WRITE
 *       Receive data from master
 *
 * Valid state transitions:
 *      __To____
 *      0  1  2  3
 * F 0| h  a  b
 * r 1| ic       d
 * o 2| e     f
 * m 3| c        g
 *
 * Transition h - Address not matched.
 *	STATE_ADDR_MATCH -> STATE_ADDR_MATCH
 *  Cond:   Pre-ack. Address doesn't match
 *  Action: NAK.
 *
 * Transition a - Address matched, write mode
 *  STATE_ADDR_MATCH -> STATE_REG_ADDR
 *	Cond:   Pre-ack. Address matches, bit0 == 0
 *	Action: ACK, Reset reg pointer.
 *
 * Transition b - Address matched, read mode
 *  STATE_ADDR_MATCH -> STATE_MASTER_READ
 *  Cond:   Pre-ack. Address matches, bit0 == 1
 *  Action: ACK.
 *
 * Transition c - Write finished
 *  STATE_XXX -> STATE_ADDR_MATCH
 *  Cond:   Stop flag is set.
 *  Action: None.
 *
 * Transition d - Initialise write
 *  STATE_REG_ADDR -> STATE_MASTER_WRITE
 *  Cond:   Pre-ack.
 *  Action: ACK, reg_ptr = USIDR.
 *
 * Transition i - Invalid reg addr
 *  STATE_REG_ADDR -> STATE_ADDR_MATCH
 *  Cond:   Pre-ack, USIDR > N_REG - 1
 *  Action: NAK.
 *
 * Transition e - Read finished
 *  STATE_MASTER_READ -> STATE_ADDR_MATCH
 *   Cond:   Post-ack. Master NAK'd.
 *   Action: None.
 *
 * Transition f - Read continues
 *  STATE_MASTER_READ -> STATE_MASTER_READ
 *   Cond:   Post-ack. Master ACK'd.
 *   Action: USIDR = *reg_ptr++
 *
 * Transition g - Write continues
 *  STATE_MASTER_WRITE -> STATE_MASTER_WRITE
 *   Cond:   Pre-ack.
 *   Action: ACK, *reg_ptr++ = USIDR
 *
 */

ISR(USI_START_vect)
{
	usi_state = 0;
	LED_ON();
	USISR = 0xF0;
}

ISR(USI_OVF_vect)
{
	static uint8_t post_ack = 0;
	/* Writing USISR directly has side effects! */
	uint8_t usisr_tmp = 0x70;
	uint8_t sda_direction;

	if (!post_ack) {
		/* Work that needs to be done before the ACK cycle */
		sda_direction = SDA_OUT;

		switch (usi_state) {
		case STATE_ADDR_MATCH:
			{
				uint8_t addr = USIDR >> 1;
				if (addr && (addr != SLAVE_ADDR)) {
					/* Transition h */
					NAK();
				} else {
					if (USIDR & 1) {
						/* Transition b */
						usi_state = STATE_MASTER_READ;
					} else {
						/* Transition a */
						reg_offset = 0;
						usi_state = STATE_REG_ADDR;
					}
					ACK();
				}
				break;
			}
		case STATE_REG_ADDR:
			if (USIDR > (N_REG - 1)) {
				/* Transition i */
				usi_state = STATE_ADDR_MATCH;
				NAK();
			} else {
				/* Transition d */
				reg_offset = USIDR;
				usi_state = STATE_MASTER_WRITE;
				ACK();
			}
			break;
		case STATE_MASTER_READ:
			USIDR = 0;
			/* Listen for master NAK */
			sda_direction = SDA_IN;
			break;
		case STATE_MASTER_WRITE:
			registers[reg_offset++] = USIDR;
			ACK();
			break;
		default:
			NAK();
		}
		/* Counter will overflow again after ACK cycle */
		usisr_tmp |= 14 << USICNT0;
		post_ack = 1;
	} else {
		/* Work that needs to be done after the ACK cycle */
		sda_direction = SDA_IN;
		switch (usi_state) {
		case STATE_MASTER_READ:
			if (USIDR) {
				/* Transition e */
				usi_state = 0;
			} else {
				/* Transition f */
				sda_direction = SDA_OUT;
				USIDR = registers[reg_offset++];
			}
			break;
		}
		post_ack = 0;
	}

	if (reg_offset > (N_REG - 1))
		reg_offset = 0;

	/* Set up SDA direction for next operation */
	if (sda_direction == SDA_OUT) {
		USI_DDR |= (1 << SDA);
	} else {
		USI_DDR &= ~(1 << SDA);
	}

	/* Clear flags and set counter */
	USISR = usisr_tmp;
}

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

void init_i2c()
{
	USICR = (1 << USISIE) | (1 << USIOIE) | (3 << USIWM0) | (1 << USICS1);	
	USI_DDR |= (1 << SCL);
	USI_DDR &= ~(1 << SDA);
	USI_PORT |= (1 << SDA) | (1 << SCL);
	USISR = 0xF0;
}

int main(void)
{
	DDRB = 0x02;
	PORTB = 0x00;

	init_i2c();
	//init_adc();
	sei();

	while (1) {
		/* Monitor for stop condition */
		if (USISR & (1 << USIPF)) {
			/* Transition c */
			usi_state = 0;
			LED_OFF();
			USISR = 0xF0;
		}
	};

	return 0;
}
