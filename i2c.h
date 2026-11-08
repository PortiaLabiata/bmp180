#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/i2c-dev.h>

typedef enum {
    OSS_0   = 0,
    OSS_1   = 1,
    OSS_2   = 2,
    OSS_3   = 3,
} Oversampling;

typedef enum {
    TEMPERATURE,
    PRESSURE,
} Mode;

typedef struct __attribute__((packed)) {
    int16_t AC1;
    int16_t AC2;
    int16_t AC3;
    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;

    int16_t B1;
    int16_t B2;

    int16_t MB;
    int16_t MC;
    int16_t MD;
} SensorE2PROM;

typedef struct {
    union {
        SensorE2PROM value;
        uint8_t buffer[sizeof(SensorE2PROM)];
    } calibration;

    int B5;
    bool initialized;
    Oversampling oss;
    Mode mode;
    int fd;
} SensorHandle;

int sensor_open(SensorHandle *s);
int sensor_init(SensorHandle *s);
int sensor_convert_temperature(SensorHandle *s);
int sensor_convert_pressure(SensorHandle *s, Oversampling oss);
int sensor_read(SensorHandle *s, float *t);
void sensor_close(SensorHandle *s);

#endif
