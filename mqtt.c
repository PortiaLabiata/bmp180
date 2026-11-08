#include <stdio.h>
#include <mosquitto.h>
#include <string.h>
#include "mqtt.h"

static const char *CLIENTID = "pressure_log";
static struct mosquitto *mosq = NULL;

int mqtt_init(void)
{
    mosquitto_lib_init();

    mosq = mosquitto_new(CLIENTID, true, NULL);
    if (!mosq)
    {
        perror("Failed to allocate MQTT client");
        return -1;
    }

    if (mosquitto_loop_start(mosq) != 0)
    {
        fprintf(stderr, "Failed to start mosquitto loop\n");
        return -1;
    }

    if (mosquitto_will_set(mosq, "luckfox/will", 0, NULL, 0, false) != 0)
    {
        fprintf(stderr, "Failed to set last will\n");
        return -1;
    }

    int ret = mosquitto_connect(mosq, "172.32.0.71", 1883, 5);
    if (ret != 0)
    {
        fprintf(stderr, "Failed to connect to MQTT broker: %s\n",
                mosquitto_strerror(ret));
        return -1;
    }

    return 0;
}

void mqtt_deinit(void)
{
    if (mosq)
    {
        mosquitto_disconnect(mosq);
        mosquitto_loop_stop(mosq, false);
        mosquitto_destroy(mosq);
        mosq = NULL;
    }
    mosquitto_lib_cleanup();
}

int mqtt_publish(const char *topic, const void *payload, size_t size)
{
    return mosquitto_publish(mosq, NULL, topic, size, payload, 0, false) == MOSQ_ERR_SUCCESS ? 0 : -1;
}
