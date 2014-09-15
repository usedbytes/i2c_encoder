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

#ifndef __I2C_SLAVE_DEFS__
#define __I2C_SLAVE_DEFS__

/* Set these appropriately for your platform */
#define USI_PORT PORTB
#define USI_DDR DDRB
#define I2C_SDA 0
#define I2C_SCL 2

/* Set these appropriately for your application */
#define I2C_N_REG 8
#define I2C_SLAVE_ADDR 0x40

/* Define anything else your application wants to know */
#define REG_CNT    i2c_reg[0]
#define REG_STATUS i2c_reg[2]
#define     STATUS_RST  (1 << 0)
#define     STATUS_CAL  (1 << 1)
#define     STATUS_CMIE (1 << 2)
#define     STATUS_CMIF (1 << 3)
#define     STATUS_LED0 (1 << 4)
#define     STATUS_LED1 (1 << 5)
#define REG_CMP    i2c_reg[3]
#define REG_MIN    i2c_reg[5]
#define REG_MAX    i2c_reg[6]
#define REG_THRESH i2c_reg[7]

#define LED_STATE_MASK (STATUS_LED0 | STATUS_LED1)
#define LED_STATE_OFF  (0 << 4)
#define LED_STATE_BIT0 (1 << 4)
#define LED_STATE_IRQ  (2 << 4)

#endif /* __I2C_SLAVE_DEFS__ */
