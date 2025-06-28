#ifndef MQTT_HELPER_H
#define MQTT_HELPER_H


#include "esp_event_base.h"
#include "esp_netif_ip_addr.h"
#include "extra_event_callback.h"
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <mqtt_client.h>
#include <stdint.h>

typedef struct{
    esp_ip4_addr_t IP;
    uint16_t Port;

} MQTT_Broker_Address_t;

typedef struct{
    esp_mqtt_client_handle_t client;
    bool isConnected;
    MQTT_Broker_Address_t broker_info;
} Mqtt_Client;

static const char *MQTT_TAG = "mqtt";
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void mqtt_init(esp_mqtt_client_handle_t *mclient, MQTT_Broker_Address_t *broker, const char* _device_id);

void extra_mqtt_data_event_register(event_callback_t callback);
void extra_mqtt_connected_event_register(event_callback_t callback);

#endif /* end of include guard: MQTT_HELPER_H */
