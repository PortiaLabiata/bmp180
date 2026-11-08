#ifndef MQTT_H
#define MQTT_H

#include <stddef.h>

int mqtt_init(void);
int mqtt_publish(const char *topic, const void *payload, size_t size);
void mqtt_deinit(void);

#endif
