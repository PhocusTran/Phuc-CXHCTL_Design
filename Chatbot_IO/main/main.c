#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "esp_netif_ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "hal/gpio_types.h"
#include "hal/uart_types.h"
#include "http_server.h"
#include "mdns_helper.h"
#include "mqtt_client.h"
#include "mqtt_helper.h"
#include "portmacro.h"
#include "soc/gpio_num.h"
#include "utils.h"
#include "wifi_helper.h"
#include "nvs_helper.h"
#include "uart_helper.h"
#include "esp_timer.h"


#define NVS_READ_SIZE 50

const uart_port_t UART_NUM = UART_NUM_0;
static QueueHandle_t uart_queue;
esp_mqtt_client_handle_t mqtt_client = {};

char* device_id = "ESP32C6";
char* output_topic_feedback;
char* in_topic;

int outs_pin[4] = {0,2,4,15};
int ins_pin[4] = {32,33,34,35};

#define PIN_OUTS ((1ULL << 15) | (1ULL <<  4) | (1ULL <<  2) | (1ULL <<  0))
#define PIN_INS  ((1ULL << 35) | (1ULL << 34) | (1ULL << 33) | (1ULL << 32))

void gpio_task(void *arg) {
    int pin_state[4] = {0,0,0,0};
    int level;
    char str[4] = "?,?";
    while (1) {

        for(int i=0;i<4;i++){
            level = gpio_get_level(ins_pin[i]);
            if(pin_state[i] != level){
                pin_state[i] = level;
                ESP_LOGI(TAG, "GPIO interrupt on pin %d %d", ins_pin[i], level);

                str[0] = i + 1 + '0';
                str[2] = level + '0';
                esp_mqtt_client_publish(mqtt_client, in_topic, str, 0, 1, 0);
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void gpio_init() {
    gpio_config_t io_config = {
        .pin_bit_mask = PIN_OUTS,  // Set GPIO19
        .mode = GPIO_MODE_INPUT_OUTPUT,                // Set as output
        .pull_up_en = GPIO_PULLUP_ENABLE,       // Disable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,   // Disable pull-down
        // .intr_type = GPIO_INTR_ANYEDGE,           // Disable interrupts
    };
    ESP_ERROR_CHECK(gpio_config(&io_config));

    io_config.pin_bit_mask = PIN_INS;
    io_config.mode = GPIO_MODE_INPUT;
    io_config.pull_up_en = GPIO_PULLUP_ENABLE;
    io_config.pull_down_en = GPIO_PULLDOWN_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_config));

    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
}


static void uart_event_task(void *pvParameters) {
    uart_event_t event;
    char data[255];

    while (1) {
        // Wait for a UART event
        if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    int len = uart_read_bytes(UART_NUM_0, data, event.size, portMAX_DELAY);
                    data[len] = '\0';  // Null-terminate string
                    // ESP_LOGI(TAG, "Message: %s", data);
                    ESP_LOG_BUFFER_HEX(TAG, data, 5);
                    ESP_LOGI(TAG, "ssid: %d", memcmp((char*)data, "ssid", (size_t)5));
                    ESP_LOGI(TAG, "pwd: %d", memcmp((char*)data, "pwd:", (size_t)4));
                    ESP_LOGI(TAG, "id: %d", memcmp((char*)data, "id:", (size_t)3));


                    if (memcmp(data, "ssid:", 5) == 0){
                        write_string_to_nvs("SSID", (char*)&data[5]);

                        char *ssid = malloc(NVS_READ_SIZE * sizeof(char));
                        read_string_from_nvs("SSID", ssid, NVS_READ_SIZE);

                        ESP_LOGE(TAG, "test: %s", ssid);
                        free(ssid);
                    }
                    if(!strncmp((char *)data, "pwd:", 4)){
                        write_string_to_nvs("PWD", (char*)&data[4]);

                        char *pwd = malloc(NVS_READ_SIZE * sizeof(char));
                        read_string_from_nvs("PWD", pwd, NVS_READ_SIZE);

                        ESP_LOGE(TAG, "test: %s", pwd);
                        free(pwd);
                    }
                    if(!strncmp((char *)data, "id:", 3)){
                        write_string_to_nvs("ID", (char*)&data[3]);

                        char *id = malloc(NVS_READ_SIZE * sizeof(char));
                        read_string_from_nvs("ID", id, NVS_READ_SIZE);

                        ESP_LOGE(TAG, "test: %s", id);
                        free(id);
                    }
                    break;

                default:
                    ESP_LOGW(TAG, "Unknown UART event: %d", event.type);
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

void on_extra_wifi_got_ip_event(void* event_data){
    ESP_LOGE(TAG, "WIFI started");
    // ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;

    MQTT_Broker_Address_t broker_addr = { };

    mdns_server_init();
    mdns_result_t *result = NULL;
    while(!result){
        result = (mdns_result_t*)query_mdns_mqtt_broker("_mqtt", "_tcp");
        if(result != NULL) break;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if(result){
        ESP_LOGI(TAG, "Found broker, port %s", result->txt[2].value);
        broker_addr.Port = (uint16_t)atoi(result->txt[2].value);

        mdns_ip_addr_t *addr = result->addr;
        while(addr){
            if(addr->addr.type == ESP_IPADDR_TYPE_V4){
                ESP_LOGI(TAG, "Found broker, IP: " IPSTR, IP2STR(&addr->addr.u_addr.ip4));
                broker_addr.IP = addr->addr.u_addr.ip4;
            }
            addr = addr->next;
        }
    }

    // MQTT initilize
    mqtt_init(&mqtt_client , &broker_addr, device_id);
}

void on_extra_mqtt_data_event(void* event_data){
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    int pin = (event->data[0] - '0') - 1;
    if(pin < 0){
        pin = 0;
    }

    gpio_set_level(outs_pin[pin], !gpio_get_level(outs_pin[pin]));

    char str[4] = "?,?";
    str[0] = pin + 1 + '0';
    str[2] = gpio_get_level(outs_pin[pin]) + '0';
    esp_mqtt_client_publish(mqtt_client, output_topic_feedback, str, 0, 1, 0);
}

void on_extra_mqtt_connected_event(void* event_data){
    char str[4] = "?,?";

    for (int i=0;i<4;i++){
        str[0] = i + 1 + '0';
        str[2] = gpio_get_level(outs_pin[i]) + '0';
        esp_mqtt_client_publish(mqtt_client, output_topic_feedback, str, 0, 1, 0);
    }

    for (int i=0;i<4;i++){
        str[0] = i + 1 + '0';
        str[2] = gpio_get_level(ins_pin[i]) + '0';
        esp_mqtt_client_publish(mqtt_client, in_topic, str, 0, 1, 0);
    }
}



void app_main(void)
{
    init_nvs();

    usart_init(UART_NUM, &uart_queue);
    xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL);

    gpio_init();

    // Initializer Wifi station
    char *ssid = malloc(NVS_READ_SIZE * sizeof(char));
    char *pwd = malloc(NVS_READ_SIZE * sizeof(char));
    char *_device_id = malloc(NVS_READ_SIZE * sizeof(char));

    read_string_from_nvs("SSID", ssid, NVS_READ_SIZE);
    read_string_from_nvs("PWD", pwd, NVS_READ_SIZE);
    read_string_from_nvs("ID", _device_id, NVS_READ_SIZE);

    if(sizeof(_device_id) > 3){
        device_id = _device_id;
        
        int len = strlen(_device_id) + 8;
        output_topic_feedback = (char*)malloc(len);
        strcpy(output_topic_feedback, _device_id);
        strcat(output_topic_feedback, "/out/fb");

        len = strlen(_device_id) + 4;
        in_topic = (char*)malloc(len);
        strcpy(in_topic, _device_id);
        strcat(in_topic, "/in");
    }

    ESP_LOGE("START", "%s %s %s", ssid, pwd, _device_id);
    
    wifi_init(ssid, pwd);
    // wifi_init("HCTL", "123456789HCTL");

    extra_wifi_event_register(on_extra_wifi_got_ip_event);
    extra_mqtt_data_event_register(on_extra_mqtt_data_event);
    extra_mqtt_connected_event_register(on_extra_mqtt_connected_event);

    // Create http server
    start_webserver();

    for(;;){
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    free(ssid);
    free(pwd);
    free(output_topic_feedback);
    free(in_topic);
}
