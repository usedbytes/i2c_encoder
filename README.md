# i2c_encoder

***NOTE***: I *will* be re-writing history in the unstable branch. I'd
recommend not using it :-)

This is an Atmel Attiny-based quadrature encoder, using the ADC to sample the
output of two sensors. This should make it very flexible, able to use any kind
of sensor which can give a varying voltage output.

It also contains a general purpose i2c library for the AVR USI. This allows
easy implementation of an i2c slave - all that is required is defining an array
to be used as the i2c "registers" and another to be used as the read/write mask
to control the access to each bit in each register. See i2c_slave_defs.c/.h for
an example.

# Register Map

| Address | Name       | Description    | Access | Reset |
|--------:|------------|:---------------|--------|------:|
|   0x00  | **CNT**    | Counter        | R/W    |    0  |
|   0x01  | ----       | Reserved       | RO     |    0  |
|   0x02  | **STATUS** | Status/Control | R/W    |    0  |
|   0x03  | **CMP**    | Compare Match  | R/W    |    0  |
|   0x04  | ----       | Reserved       | RO     |    0  |
|   0x05  | **MIN**    | Min level      | RO     |    0  |
|   0x06  | **MAX**    | Max level      | RO     | 0xFF  |
|   0x07  | **THRESH** | Threshold      | R/W    | 0x80  |

# Register Descriptions

## **CNT**
The count register is an 8-bit counter which tracks the current encoder value.
It will overflow at 255 -> 0 (counting up) and 0 -> 255 (counting down).

The counter can be written to, though this may not have the desired result due
to the internal phase state of the counter. The safest way to set the counter
value is to use the *RST* bit in the **STATUS** register.

## **STATUS**
The status/control register controls operation of the system. It can be read or
written at any time.

| RSVD | RSVD | LED1 | LED0 | CMIF | CMIE | CAL  | RST  |
|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|
|    7 |    6 |    5 |    4 |    3 |    2 |    1 |    0 |
|    r |    r |   rw |   rw |   rw |   rw |   rw |   rw |

### *RST*
Writing a 1 to this bit will reset the counter, and set the **CNT** register to
zero. If the **CMP** register is set to zero, and the compare-match interrupt
is enabled then an interrupt will be generated.

This bit will be automatically cleared once the reset has completed.

### *CAL*
Writing a one to this bit enters calibration mode. For details on calibration
operation see the section on Calibration.

Writing a zero to this bit will exit calibration mode and cause recalculation
of the threshold value (**THRESH**)

### *CMIE*
Enables the compare-match interrupt. This bit must be set and cleared by
software.

If this bit is set, and *LED[1:0]* are set to enable the interrupt output, then
the LED/IRQ pin will go HIGH when the counter value matches the value set in
**CMP**.

### *CMIF*
The compare-match interrupt flag. This bit will be asserted when a
compare-match occurs. The host must write 0 to this bit to clear the flag and
release the LED/IRQ line. Writing 1 to this bit forces a compare match event,
which will assert the LED/IRQ line depending on the *LED[1:0]* setting.

### *LED[1:0]*
These two bits control the behaviour of the LED/IRQ output, as described in the
table below

| LED[1:0] | Meaning                                                           |
|----------|-------------------------------------------------------------------|
| 00       | Off - The LED/IRQ pin is not used                                 |
| 01       | Pulse mode - The LED/IRQ pin follows the LSB of the counter       |
| 10       | IRQ Mode - The LED/IRQ pin is used for the compare-match interrupt|
| 11       | Reserved                                                          |

### *RSVD*
These bits are reserved for future use, and should always be written zero.

