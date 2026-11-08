#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "i2c.h"
#include "mqtt.h"

static volatile sig_atomic_t stop = 0;

void sighandler(int signum)
{
    (void)signum;
    stop = 1;
}

int publish_float(const char *topic, float v)
{
    char buffer[128] = {0};
    int size = snprintf(buffer, 128, "%.3f", v);
    return mqtt_publish(topic, buffer, size);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    signal(SIGTERM, sighandler);
    signal(SIGINT, sighandler);

    if (mqtt_init() < 0)
    {
        return -1;
    }

    int ret = 0;
    SensorHandle handle = {0};
    if (sensor_open(&handle) < 0)
    {
        ret = -1;
        goto cleanup;
    }

    if (sensor_init(&handle) < 0)
    {
        ret = -1;
        goto cleanup;
    }

    while (!stop)
    {
        if (sensor_convert_temperature(&handle) < 0)
        {
            ret = -1;
            break;
        }

        usleep(5000);
        float temperature = 0.0f;
        if (sensor_read(&handle, &temperature) < 0)
        {
            ret = -1;
            break;
        }

        if (publish_float("luckfox/env/temperature", temperature) < 0)
        {
            fprintf(stderr, "Failed to send data\n");
        }

        float pressure = 0.0f;
        if (sensor_convert_pressure(&handle, OSS_0) < 0)
        {
            ret = -1;
            break;
        }
        usleep(5000);

        if (sensor_read(&handle, &pressure) < 0)
        {
            ret = -1;
            break;
        }

        if (publish_float("luckfox/env/pressure", pressure) < 0)
        {
            fprintf(stderr, "Failed to send data\n");
        }

        sleep(1);
    }

cleanup:
    mqtt_deinit();
    sensor_close(&handle);
    return ret;
}
