
TARGET = test_i2c_slave
SRC = $(TARGET).c i2c_slave_defs.c
OBJS = $(patsubst %.c,%.o,$(SRC))

CC = gcc
CFLAGS := -Wall -g -I..
LDFLAGS :=

# Look for .c dependencies in ..
vpath %.c ..

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

clean:
	rm *.o $(TARGET)

.PHONY: clean
