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

#include <stdint.h>

#include "i2c_slave_defs.h"

/* Set the initial register values */
volatile uint8_t i2c_reg[I2C_N_REG] = {    0,    0,    0,    0,
                                           0,    0, 0xFF, 0x80 };
/* Set the register write masks. A 1 indicates a bit can be written */
const uint8_t i2c_w_mask[I2C_N_REG] = { 0xFF,    0, 0x3F, 0xFF,
                                           0,    0,    0, 0xFF };

