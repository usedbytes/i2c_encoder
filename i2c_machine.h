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

#ifndef __I2C_MACHINE__
#define __I2C_MACHINE__

#include <stdint.h>
#include "i2c_slave_defs.h"

/* Initialise the USI and I2C state machine */
void i2c_init(void);
/*
 * Check for and handle a stop condition.
 * Returns non-zero if any registers have been changed
 */
uint8_t i2c_check_stop(void);

/*
 * These need to be instantiated in i2c_slave_defs.c,
 * I2C_N_REG should be defined in i2c_slave_defs.h
 */
extern volatile uint8_t i2c_reg[I2C_N_REG];
extern const uint8_t i2c_w_mask[I2C_N_REG];

#endif /* __I2C_MACHINE__ */
