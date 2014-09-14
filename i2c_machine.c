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

#define DEBUG

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "i2c_slave_defs.h"
#include "i2c_machine.h"

#define I2C_SDA_DIR_OUT 1
#define I2C_SDA_DIR_IN 0

#define NAK() USIDR = 0x80
#define ACK() USIDR = 0x00

#define I2C_STATE_ADDR_MATCH   0
#define I2C_STATE_REG_ADDR     1
#define I2C_STATE_MASTER_READ  2
#define I2C_STATE_MASTER_WRITE 3

#ifdef DEBUG
#define LED_ON() PORTB |= 0x2
#define LED_OFF() PORTB &= ~0x2
#define LED_FLICKER() LED_OFF(); LED_ON()
#else
#define LED_ON()
#define LED_OFF()
#define LED_FLICKER()
#endif

volatile uint8_t i2c_update = 0;
volatile uint8_t i2c_state = 0;
volatile uint8_t i2c_offset = 0;

/* USI i2c Slave State Machine
 * ===========================
 *
 * 4 States:
 *     0 I2C_STATE_ADDR_MATCH
 *       Waiting for address (idle)
 *
 *     1 I2C_STATE_REG_ADDR
 *       Receive register address*
 *
 *     2 I2C_STATE_MASTER_READ
 *       Transmit data to master
 *
 *     3 I2C_STATE_MASTER_WRITE
 *       Receive data from master
 *
 * Valid state transitions:
 *      __To______
 *      0  1  2  3
 * F 0| h  a  b
 * r 1| ic       d
 * o 2| e     f
 * m 3| c        g
 *
 * Transition h - Address not matched.
 *  I2C_STATE_ADDR_MATCH -> I2C_STATE_ADDR_MATCH
 *  Cond:   Pre-ack. Address doesn't match
 *  Action: NAK.
 *
 * Transition a - Address matched, write mode
 *  I2C_STATE_ADDR_MATCH -> I2C_STATE_REG_ADDR
 *  Cond:   Pre-ack. Address matches, bit0 == 0
 *  Action: ACK, Reset reg pointer.
 *
 * Transition b - Address matched, read mode
 *  I2C_STATE_ADDR_MATCH -> I2C_STATE_MASTER_READ
 *  Cond:   Pre-ack. Address matches, bit0 == 1
 *  Action: ACK.
 *
 * Transition c - Write finished
 *  I2C_STATE_XXX -> I2C_STATE_ADDR_MATCH
 *  Cond:   Stop flag is set.
 *  Action: None.
 *
 * Transition d - Initialise write
 *  I2C_STATE_REG_ADDR -> I2C_STATE_MASTER_WRITE
 *  Cond:   Pre-ack.
 *  Action: ACK, reg_ptr = USIDR.
 *
 * Transition i - Invalid reg addr
 *  I2C_STATE_REG_ADDR -> I2C_STATE_ADDR_MATCH
 *  Cond:   Pre-ack, USIDR > N_REG - 1
 *  Action: NAK.
 *
 * Transition e - Read finished
 *  I2C_STATE_MASTER_READ -> I2C_STATE_ADDR_MATCH
 *  Cond:   Post-ack. Master NAK'd.
 *  Action: None.
 *
 * Transition f - Read continues
 *  I2C_STATE_MASTER_READ -> I2C_STATE_MASTER_READ
 *  Cond:   Post-ack. Master ACK'd.
 *  Action: USIDR = *reg_ptr++
 *
 * Transition g - Write continues
 *  I2C_STATE_MASTER_WRITE -> I2C_STATE_MASTER_WRITE
 *  Cond:   Pre-ack.
 *  Action: ACK, *reg_ptr++ = USIDR
 *
 */

ISR(USI_START_vect)
{
	i2c_state = 0;
	LED_ON();
	USISR = 0xF0;
}

ISR(USI_OVF_vect)
{
	static uint8_t post_ack = 0;
	/* Writing USISR directly has side effects! */
	uint8_t usisr_tmp = 0x70;
	uint8_t sda_direction;
	uint8_t tmp;

	if (!post_ack) {
		/* Work that needs to be done before the ACK cycle */
		sda_direction = I2C_SDA_DIR_OUT;

		switch (i2c_state) {
		case I2C_STATE_ADDR_MATCH:
			tmp = USIDR >> 1;
			if (tmp && (tmp != I2C_SLAVE_ADDR)) {
				/* Transition h */
				NAK();
			} else {
				if (USIDR & 1) {
					/* Transition b */
					i2c_state = I2C_STATE_MASTER_READ;
				} else {
					/* Transition a */
					i2c_offset = 0;
					i2c_state = I2C_STATE_REG_ADDR;
				}
				ACK();
			}
			break;
		case I2C_STATE_REG_ADDR:
			if (USIDR > (I2C_N_REG - 1)) {
				/* Transition i */
				i2c_state = I2C_STATE_ADDR_MATCH;
				NAK();
			} else {
				/* Transition d */
				i2c_offset = USIDR;
				i2c_state = I2C_STATE_MASTER_WRITE;
				ACK();
			}
			break;
		case I2C_STATE_MASTER_READ:
			USIDR = 0;
			/* Listen for master NAK */
			sda_direction = I2C_SDA_DIR_IN;
			break;
		case I2C_STATE_MASTER_WRITE:
			tmp = i2c_w_mask[i2c_offset];
			if (tmp) {
				/* Only heed writeable bits */
				i2c_reg[i2c_offset] &= ~tmp;
				i2c_reg[i2c_offset] |= USIDR & tmp;
				i2c_update++;
			}
			i2c_offset++;
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
		sda_direction = I2C_SDA_DIR_IN;
		switch (i2c_state) {
		case I2C_STATE_MASTER_READ:
			if (USIDR) {
				/* Transition e */
				i2c_offset = 0;
				i2c_state = 0;
			} else {
				/* Transition f */
				sda_direction = I2C_SDA_DIR_OUT;
				USIDR = i2c_reg[i2c_offset++];
			}
			break;
		}
		post_ack = 0;
	}

	if (i2c_offset > (I2C_N_REG - 1))
		i2c_offset = 0;

	/* Set up SDA direction for next operation */
	if (sda_direction == I2C_SDA_DIR_OUT) {
		USI_DDR |= (1 << I2C_SDA);
	} else {
		USI_DDR &= ~(1 << I2C_SDA);
	}

	/* Clear flags and set counter */
	USISR = usisr_tmp;
}

void i2c_init()
{
	i2c_state = 0;
	USICR = (1 << USISIE) | (1 << USIOIE) | (3 << USIWM0) | (1 << USICS1);
	USI_DDR |= (1 << I2C_SCL);
	USI_DDR &= ~(1 << I2C_SDA);
	USI_PORT |= (1 << I2C_SDA) | (1 << I2C_SCL);
	USISR = 0xF0;
}

uint8_t i2c_check_stop()
{
	uint8_t ret = 0;

	if (USISR & (1 << USIPF)) {
		/* Transition c */
		if ((i2c_state == I2C_STATE_MASTER_WRITE) && i2c_update)
			i2c_offset = 0;
		i2c_state = I2C_STATE_ADDR_MATCH;
		LED_OFF();
		USISR = 0xF0;

		ret = i2c_update;
		i2c_update = 0;
	}

	return ret;
}
