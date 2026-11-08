#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include <sys/ioctl.h>
#include "i2c.h"

static const char *I2C_PATH = "/dev/i2c-3";
static const int SLAVE_ADDR = 0x77;

enum reg {
    OUT_XLSB    = 0xf8,
    OUT_LSB     = 0xf7,
    OUT_MSB     = 0xf6,
    CTRL_MEAS   = 0xf4,
    SOFT_RESET  = 0xe0,
    ID          = 0xd0,
    AC1         = 0xaa,
};

int sensor_open(SensorHandle *s)
{
    int fd = open(I2C_PATH, O_RDWR);

    s->initialized = false;
    s->mode = TEMPERATURE;
    s->oss = OSS_0;

    if (fd < 0)
    {
        perror("Failed to open I2C bus");
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, SLAVE_ADDR) < 0)
    {
        perror("Failed to set slave address");
        return -1;
    }

    s->fd = fd;
    return 0;
}

void sensor_close(SensorHandle *s)
{
    if (s->fd > 0)
    {
        close(s->fd);
    }
}

static int sensor_write_reg(SensorHandle *s, uint8_t reg, uint8_t value)
{
    assert(s->fd > 0);

    char buf[2] = {0};
    buf[0] = reg;
    buf[1] = value;

    int ret = write(s->fd, buf, 2);
    if (ret < 0)
    {
        perror("Failed to write data");
        return -1;
    }
    return ret;
}

static int sensor_read_reg(SensorHandle *s, uint8_t reg, uint8_t *value)
{
    assert(s->fd > 0);

    if (write(s->fd, &reg, 1) < 0)
    {
        perror("Failed to write register");
        return -1;
    }

    int ret = read(s->fd, value, 1);
    if (ret < 0)
    {
        perror("Failed to read data");
        return -1;
    }
    return ret;
}

static int sensor_read_burst(SensorHandle *s, uint8_t addr, uint8_t *buf, size_t size)
{
    assert(s->fd > 0);

    if (write(s->fd, &addr, 1) < 0)
    {
        perror("Failed to write register");
        return -1;
    }

    int ret = read(s->fd, buf, size);
    if (ret < 0)
    {
        perror("Failed to read data");
        return -1;
    }
    return ret;
}

int sensor_init(SensorHandle *s)
{
    int ret = sensor_read_burst(s, AC1, s->calibration.buffer, sizeof(s->calibration.buffer));
    if (ret < 0)
    {
        return -1;
    }

    for (unsigned int i = 0; i < sizeof(s->calibration.buffer); i += 2) 
    {
        uint8_t tmp = s->calibration.buffer[i]; 
        s->calibration.buffer[i] = s->calibration.buffer[i + 1]; 
        s->calibration.buffer[i + 1] = tmp;
    }
    return ret;
}

int sensor_convert_temperature(SensorHandle *s)
{
    s->mode = TEMPERATURE;
    return sensor_write_reg(s, CTRL_MEAS, 0x2e);
}

int sensor_convert_pressure(SensorHandle *s, Oversampling oss)
{
    assert(s->fd > 0);
    assert(oss == OSS_0 || oss == OSS_1 || oss == OSS_2 || oss == OSS_3);

    s->oss = oss;
    s->mode = PRESSURE;
    uint8_t value = 0x00;
    switch (oss)
    {
        case OSS_0:
            value = 0x34;
            break;
        case OSS_1:
            value = 0x74;
            break;
        case OSS_2:
            value = 0xB4;
            break;
        case OSS_3:
            value = 0xF4;
            break;
            
    }
    return sensor_write_reg(s, CTRL_MEAS, value);
}

static void calc_temperature(SensorHandle *s, uint16_t uncomp, float *v)
{
    SensorE2PROM *eeprom = &s->calibration.value;
    int x1 = (uncomp - eeprom->AC6) * eeprom->AC5 / 32768;
    int x2 = eeprom->MC * 2048 / (x1 + eeprom->MD);
    int b5 = x1 + x2;
    s->B5 = b5;
    *v = ((float)(b5) + 8.0f) / 160.0f;
}

static void calc_pressure(SensorHandle *s, int32_t uncomp, float *v)
{
    // I hate this with all my being
    SensorE2PROM *eeprom = &s->calibration.value;
    int oss = s->oss;

    int32_t b6 = s->B5 - 4000;
    int32_t x1 = (eeprom->B2 * ((b6 * b6) >> 12)) >> 11;
    int32_t x2 = (eeprom->AC2 * b6) >> 11;
    int32_t x3 = x1 + x2;
    int32_t b3 = ((((int32_t)eeprom->AC1 * 4 + x3) << oss) + 2) / 4;

    x1 = (eeprom->AC3 * b6) >> 13;
    x2 = (eeprom->B1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    uint32_t b4 = ((uint32_t)eeprom->AC4 * (uint32_t)(x3 + 32768)) >> 15;
    uint32_t b7 = (uncomp - (uint32_t)b3) * (uint32_t)(50000 >> oss);

    int32_t p;
    if (b7 < 0x80000000u)
    {
        p = (int32_t)((b7 * 2) / b4);
    }
    else
    {
        p = (int32_t)((b7 / b4) * 2);
    }

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p += (x1 + x2 + 3791) >> 4;

    *v = (float)p;
}

int sensor_read(SensorHandle *s, float *t)
{
    uint8_t xlsb, lsb, msb;
    if (sensor_read_reg(s, OUT_XLSB, &xlsb) < 0)
    {
        return -1;
    }

    if (sensor_read_reg(s, OUT_LSB, &lsb) < 0)
    {
        return -1;
    }

    if (sensor_read_reg(s, OUT_MSB, &msb) < 0)
    {
        return -1;
    }

    if (s->mode == TEMPERATURE)
    {
        uint16_t uv = ((int16_t)msb << 8) | lsb;
        calc_temperature(s, uv, t);
    } 
    else if (s->mode == PRESSURE)
    {
        int32_t uv = (((int32_t)msb << 16) | ((int32_t)lsb << 8) | xlsb) >> (8 - s->oss);
        calc_pressure(s, uv, t);
    }

    return 0;
}
