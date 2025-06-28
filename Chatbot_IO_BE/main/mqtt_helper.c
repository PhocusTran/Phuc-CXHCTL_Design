#include "mqtt_helper.h"
#include "esp_log.h"
#include "extra_event_callback.h"
#include "mqtt_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* topic = NULL;
void trigger_extra_on_data_event(void* event_data);
void trigger_extra_on_connected_event(void* event_data);

void generate_info_message(char* info, int len, char* _device_id){
    strcpy(info, _device_id);

    info[len] = '.';
    info[len + 1] = 4 + '0';
    info[len + 2] = '.';
    info[len + 3] = 4 + '0';
    info[len + 4] = '\0';
}

void mqtt_init(esp_mqtt_client_handle_t *mclient, MQTT_Broker_Address_t *broker, const char* _device_id){
    char result[50];
    char ip_str[16]; // Buffer for the IP address string

    int id_len = strlen(_device_id);
    char* LWT_topic = (char*)malloc(id_len + 5);
    strcpy(LWT_topic, _device_id);
    strcat(LWT_topic, "/LWT");

    // Convert esp_ip4_addr_t to char*
    esp_ip4addr_ntoa(&broker->IP, ip_str, sizeof(ip_str));

    sprintf(result,"mqtt://%s:%d", ip_str, broker->Port);
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = result,
        .credentials.client_id = _device_id,
        .session.last_will.topic = LWT_topic,
        .session.last_will.msg = "off",
        .session.keepalive = 5,
    };
    esp_mqtt_client_init(&mqtt_cfg); *mclient = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(*mclient, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(*mclient);

    // you're here
    if(topic == NULL){
        topic = malloc(sizeof(_device_id)+1);
        strcpy(topic, _device_id);
    }
    // ESP_LOGI("b", "%s", topic);
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    int msg_id;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:

            char *info_topic = (char*)malloc(strlen(topic) + 6);
            char *out_topic = (char*)malloc(strlen(topic) + 5);

            strcpy(info_topic, topic);
            strcpy(out_topic, topic);

            ESP_LOGI(MQTT_TAG, "2 %s %s", info_topic, out_topic);

            strcat(info_topic, "/info");
            strcat(out_topic, "/out");

            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");

            int len = strlen(topic);
            char* info = (char*)malloc(len + 6);
            generate_info_message(info, len, topic);

            esp_mqtt_client_publish(client, info_topic, info, 0, 1, 0);
            free(info);

            esp_mqtt_client_subscribe(client, out_topic, 0);

            trigger_extra_on_connected_event(event_data);

            free(info_topic);
            free(out_topic);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI("SUB", "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI("PUB", "MQTT_EVENT_PUBLISHED");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGE("MQTT", "MQTT DISCONNECTED");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI("DATA", "MQTT_EVENT_DATA");
            trigger_extra_on_data_event(event_data);
            break;
    }
}

extra_event_callback extra_mqtt_on_data = {0};
void extra_mqtt_data_event_register(event_callback_t callback){
    if(extra_mqtt_on_data.callback_registered_count < MAX_CALLBACK){
        extra_mqtt_on_data.callback[extra_mqtt_on_data.callback_registered_count++] = callback;
    }
    else return;
}

extra_event_callback extra_mqtt_connected = {0};
void extra_mqtt_connected_event_register(event_callback_t callback){
    if(extra_mqtt_connected.callback_registered_count < MAX_CALLBACK){
        extra_mqtt_connected.callback[extra_mqtt_connected.callback_registered_count++] = callback;
    }
    else return;
}

void trigger_extra_on_connected_event(void* event_data){
    if(extra_mqtt_connected.callback_registered_count == 0) return;

    for(int i = 0; i < extra_mqtt_connected.callback_registered_count; i++){
        extra_mqtt_connected.callback[i](event_data);
    }
}

void trigger_extra_on_data_event(void* event_data){
    if(extra_mqtt_on_data.callback_registered_count == 0) return;

    for(int i = 0; i < extra_mqtt_on_data.callback_registered_count; i++){
        extra_mqtt_on_data.callback[i](event_data);
    }
}
