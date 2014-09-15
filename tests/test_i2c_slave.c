#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>

#include "i2c_slave_defs.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x[0])))
#define DEVICENO 0

extern volatile uint8_t i2c_reg[I2C_N_REG];
extern const uint8_t i2c_w_mask[I2C_N_REG];
uint8_t initial_reg[I2C_N_REG];

void dump_i2c_msg(struct i2c_msg *msg)
{
	int i;
	printf("Addr:\t0x%02x [%c]\n", msg->addr,
			msg->flags & I2C_M_RD ? 'R' : 'W');
	printf("Flags:\t0x%02x\n", msg->flags);
	printf(" len:\t%d\n", msg->len);
	printf("data:\t");
	for (i = 0; i < msg->len; i++) {
		printf("0x%02x ", msg->buf[i]);
	}
	printf("\n");
}

int test_read(int fd)
{
	int res, i;
	uint8_t buf[I2C_N_REG] = { 0 };
	struct i2c_msg msg = {
		.addr = I2C_SLAVE_ADDR,
		.flags = I2C_M_RD,
		.len = I2C_N_REG,
		.buf = buf
	};
	struct i2c_rdwr_ioctl_data arg = { &msg, 1 };

	res = ioctl(fd, I2C_RDWR, &arg);
	if (res != arg.nmsgs) {
		if (res < 0)
			printf("Ioctl error: %i '%s'\n", res, strerror(errno));
		else
			printf("Invalid number of messages (%i != %i)\n", res, arg.nmsgs);
	} else {
		res = 0;
	}

	for (i = 0; i < I2C_N_REG; i++) {
		if (buf[i] != initial_reg[i]) {
			printf("Register content mismatch at 0x%02x: expected 0x%02x, got 0x%02x\n",
					i, initial_reg[i], buf[i]);
			res = 1;
		}
	}

	return res;
}

int test_read_individual(int fd)
{
	int res, i;
	uint8_t write_buf[1] = { 0 };
	uint8_t read_buf[1] = { 0 };
	struct i2c_msg msgs[] = {
		{
			.addr = I2C_SLAVE_ADDR,
			.len = 1,
			.buf = write_buf
		},
		{
			.addr = I2C_SLAVE_ADDR,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = read_buf,
		},
	};
	struct i2c_rdwr_ioctl_data arg = { msgs, 2 };

	for (i = 0; i < I2C_N_REG; i++) {
		write_buf[0] = i;
		res = ioctl(fd, I2C_RDWR, &arg);
		if (res != arg.nmsgs) {
			if (res < 0) {
				printf("Ioctl error, reg %d: %i '%s'\n", i, res,
						strerror(errno));
				break;
			} else {
				printf("Invalid number of messages (%i != %i)\n",
						res, arg.nmsgs);
			}
		} else {
			res = 0;
		}

		if (read_buf[0] != initial_reg[i]) {
			printf("Register content mismatch at 0x%02x: expected 0x%02x, got 0x%02x\n",
					i, initial_reg[i], read_buf[0]);
			res = 1;
			break;
		}
	}

	return res;
}

int test_write(int fd)
{
	int res, i;
	uint8_t buf[I2C_N_REG + 1] = { 0 };
	struct i2c_msg msg = {
		.addr = I2C_SLAVE_ADDR,
		.len = I2C_N_REG + 1,
		.buf = buf
	};
	struct i2c_rdwr_ioctl_data arg = { &msg, 1 };

	for (i = 0; i < I2C_N_REG; i++) {
		buf[i + 1] = initial_reg[i];
	}

	res = ioctl(fd, I2C_RDWR, &arg);
	if (res != arg.nmsgs) {
		if (res < 0)
			printf("Ioctl error: %i '%s'\n", res, strerror(errno));
		else
			printf("Invalid number of messages (%i != %i)\n", res, arg.nmsgs);
	} else {
		res = 0;
	}

	return res;
}

int test_write_individual(int fd)
{
	int res, i;
	uint8_t buf[2] = { 0 };
	struct i2c_msg msg = {
		.addr = I2C_SLAVE_ADDR,
		.len = 2,
		.buf = buf
	};
	struct i2c_rdwr_ioctl_data arg = { &msg, 1 };

	for (i = 0; i < I2C_N_REG; i++) {
		buf[0] = i;
		buf[1] = initial_reg[i];
		res = ioctl(fd, I2C_RDWR, &arg);
		if (res != arg.nmsgs) {
			if (res < 0)
				printf("Ioctl error (%i): %i '%s'\n", i, res, strerror(errno));
			else
				printf("Invalid number of messages (%i != %i)\n", res, arg.nmsgs);
		} else {
			res = 0;
		}
	}

	return res;
}

struct test {
	char name[255];
	int (*func)(int fd);
	int result;
};

#define TEST(__func) { .name = #__func, .func = __func }

struct test tests[] = {
	TEST(test_write),
	TEST(test_write_individual),
	TEST(test_read),
	TEST(test_read_individual),
};

int main(int argc, char *argv[])
{
	int i, fd;
	char filename[255];

	snprintf(filename, 255, "/dev/i2c-%d", DEVICENO);
	printf("Opening %s\n", filename);
	fd = open(filename, O_RDWR);
	if (fd < 0) {
		printf("Failed opening device %d, '%s'\n", DEVICENO, strerror(errno));
		return 1;
	}

	for (i = 0; i < I2C_N_REG; i++) {
		initial_reg[i] = i2c_reg[i];
	}

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		struct test *t = &tests[i];
		printf("Running %s...\n", t->name);
		t->result = t->func(fd);
		if (t->result)
			break;
	}

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		struct test *t = &tests[i];
		printf("%30s: [%s]\n", t->name, t->result ? "FAIL" : "PASS");
	}

	return 0;
}

