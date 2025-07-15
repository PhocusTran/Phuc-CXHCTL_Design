#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "wifi_server.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_wifi_types_generic.h"
#include "nvs_flash.h"
#include "esp_wifi_default.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "file_server.h"
#include "mdns.h"

static const char* TAG = "HTTP";
extern char hostname[25];
extern uint8_t restart_usb_after_write;

extern char ip_str[16];

void mdns_server_init();


static const char* mDNS_instance_name = "USB WIFI";
void mdns_server_init(){
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set(mDNS_instance_name));

    ESP_ERROR_CHECK(mdns_service_add("http", "_http", "_tcp", 80, NULL, 0));
    ESP_LOGI("mDNS", "mDNS service started at %s", hostname);
}

static int wifi_retry_cnt = 0;
void wifi_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    // printf("ID: %s %d\n", base, (int)id);
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED){
        ESP_LOGI("W_EVENT", "%s", "connected");
    } else if(base == WIFI_EVENT && id == WIFI_EVENT_STA_START){
        esp_wifi_connect();
    } else if(base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED){
        esp_wifi_connect();
        ESP_LOGW(TAG, "Wifi retry %d.", wifi_retry_cnt++);
    }

    else if (base == IP_EVENT){
        if (id == IP_EVENT_STA_GOT_IP){
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            sprintf(ip_str, IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI("IP_EVENT ", "Got ip: %s", ip_str);

            /* start file server when obtain IP from AP */
            start_file_server();
            if(hostname[0]!='\0') mdns_server_init();
        }
    }
}


esp_err_t wifi_init(const char* ssid, const char* pwd){
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();


    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    esp_wifi_init(&wifi_init_config);

    /* change default configuration to max out wifi performance */
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_bandwidth(WIFI_MODE_STA, WIFI_BW80);
    esp_wifi_set_max_tx_power(70);

    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);


    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    // wifi_config.sta.failure_retry_cnt = 10;

    strncpy((char*)wifi_config.sta.ssid, ssid, strlen(ssid));
    strncpy((char*)wifi_config.sta.password, pwd, strlen(pwd));
    wifi_config.sta.ssid[strlen(ssid)] = '\0';
    wifi_config.sta.password[strlen(pwd)] = '\0';
    printf("SSID %s\n", wifi_config.sta.ssid);
    printf("PSWD %s\n", wifi_config.sta.password);

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
    return ESP_OK;
}

esp_err_t ping_handler(httpd_req_t *req) {
    httpd_resp_sendstr(req, "hello world\n\r");
    ESP_LOGI(TAG, "HELLO WORLD");
    return ESP_OK;
}
