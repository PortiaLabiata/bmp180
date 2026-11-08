SHELL := /bin/bash
TOOLCHAIN := /home/portia_labiata/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin
CC := $(TOOLCHAIN)/arm-rockchip830-linux-uclibcgnueabihf-gcc
LD := $(TOOLCHAIN)/arm-rockchip830-linux-uclibcgnueabihf-gcc
LIB := /home/portia_labiata/luckfox-pico/sysdrv/source/buildroot/buildroot-2023.02.6/output/target/usr/lib

INCS := /home/portia_labiata/luckfox-pico/sysdrv/source/buildroot/buildroot-2023.02.6/output/build/mosquitto-2.0.18/include
LIBS := mosquitto ssl crypto
CFLAGS := -Wall -Wextra -Wpedantic -Werror -O3 $(addprefix -I,$(INCS))
LDFLAGS := -L$(LIB) $(addprefix -l,$(LIBS))

SRCS := main.c i2c.c mqtt.c
OBJS := $(SRCS:.c=.o)
TARGET := bmp180

.PHONY: all clean flash

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

flash: $(TARGET)
	adb push $(TARGET) /opt

clean:
	rm $(OBJS) $(TARGET)
