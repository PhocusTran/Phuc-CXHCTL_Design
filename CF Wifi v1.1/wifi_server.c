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
#include "nvs.h"       // Thêm include này
#include "esp_wifi_default.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "file_server.h" // For start_file_server() and url_decode()
#include "mdns.h"
#include "main.h" // For current_wifi_op_mode_t, MAX_LEN, NVS_NAMESPACE/KEYS

static const char* TAG = "HTTP";
extern char hostname[MAX_LEN + 1]; // Đã sửa kích thước buffer
extern uint8_t restart_usb_after_write;

extern char ip_str[16];

extern current_wifi_op_mode_t current_wifi_op_mode;

static httpd_handle_t config_server_handle = NULL; // Handle for the config server

void mdns_server_init();


static const char* mDNS_instance_name = "USB WIFI";
void mdns_server_init(){
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set(mDNS_instance_name));

    ESP_ERROR_CHECK(mdns_service_add("http", "_http", "_tcp", 80, NULL, 0));
    ESP_LOGI("mDNS", "mDNS service started at %s", hostname);
}

#define WIFI_MAX_RETRY 5
static int wifi_retry_cnt = 0;

static void start_wifi_ap_and_config_server(void) {
    ESP_LOGI(TAG, "Starting WiFi in AP mode...");
    wifi_init_ap(); // This will also start the config server
    current_wifi_op_mode = WIFI_MODE_AP_ACTIVE;
}

void wifi_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
            ESP_LOGI(TAG, "STA start, attempting to connect...");
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "STA disconnected. Retry count: %d/%d", wifi_retry_cnt, WIFI_MAX_RETRY);
            if (wifi_retry_cnt < WIFI_MAX_RETRY) {
                esp_wifi_connect();
                wifi_retry_cnt++;
            } else {
                ESP_LOGE(TAG, "STA connect failed too many times. Switching to AP mode.");
                start_wifi_ap_and_config_server(); // Switch to AP mode and start config server
            }
        } else if (id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "STA connected.");
            wifi_retry_cnt = 0; // Reset retry count on successful connection
        } else if (id == WIFI_EVENT_AP_START) {
            ESP_LOGI(TAG, "AP started.");
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            sprintf(ip_str, IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Got IP: %s", ip_str);

            current_wifi_op_mode = WIFI_MODE_STA_ACTIVE;
            // Stop config server if it was running (e.g., switched from AP to STA)
            if (config_server_handle) {
                httpd_stop(config_server_handle);
                config_server_handle = NULL;
                ESP_LOGI(TAG, "Config HTTP server stopped.");
            }
            start_file_server(); // Start the main file server
            if(hostname[0]!='\0') mdns_server_init();
        } else if (id == IP_EVENT_AP_STAIPASSIGNED) {
            ESP_LOGI(TAG, "AP: client connected.");
        }
    }
}

// Function to start WiFi in STA mode
esp_err_t wifi_init_sta(const char* ssid, const char* pwd) {
    // NVS init should be done once in app_main
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_bandwidth(WIFI_MODE_STA, WIFI_BW80);
    esp_wifi_set_max_tx_power(70);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    
    // Copy SSID and Password
    strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, pwd, sizeof(wifi_config.sta.password));
    
    ESP_LOGI(TAG, "STA SSID: %s", wifi_config.sta.ssid);

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    return ESP_OK;
}

// Function to start WiFi in AP mode
esp_err_t wifi_init_ap(void) {
    // NVS init should be done once in app_main
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32_CF_Config", // Default AP SSID
            .ssid_len = strlen("ESP32_CF_Config"),
            .channel = 1,
            .password = "", // No password for initial setup
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_LOGI(TAG, "Setting WiFi to AP mode with SSID: %s", wifi_config.ap.ssid);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    // Start config server
    return start_config_server();
}

// Implement the new save_config_to_nvs function
esp_err_t save_config_to_nvs(const char *ssid_val, const char *pwd_val, const char *hostname_val, uint8_t remount_flag, int delay_val) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for write!", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, NVS_KEY_SSID, ssid_val);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set SSID in NVS (%s)", esp_err_to_name(err));

    err = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, pwd_val);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set Password in NVS (%s)", esp_err_to_name(err));

    err = nvs_set_str(nvs_handle, NVS_KEY_HOSTNAME, hostname_val);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set Hostname in NVS (%s)", esp_err_to_name(err));

    err = nvs_set_u8(nvs_handle, NVS_KEY_REMOUNT, remount_flag);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set Remount flag in NVS (%s)", esp_err_to_name(err));

    err = nvs_set_i32(nvs_handle, NVS_KEY_DELAY, delay_val);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set Delay in NVS (%s)", esp_err_to_name(err));

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) committing NVS!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Configuration saved to NVS successfully.");
    }

    nvs_close(nvs_handle);
    return err; // Return the result of commit
}


// Handler for the config HTML form (GET /)
static esp_err_t config_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    const char *html_form = 
        "<!DOCTYPE html>"
        "<html lang='en'>"
        "<head>"
        "    <meta charset='UTF-8'>"
        "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "    <title>ESP32-CF Configuration</title>"
        "    <style>"
        "        body { font-family: sans-serif; margin: 20px; background-color: #f4f7f6; color: #333; }"
        "        .container { max-width: 500px; margin: 0 auto; background-color: #fff; padding: 25px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
        "        h1 { color: #0056b3; text-align: center; margin-bottom: 25px; }"
        "        label { display: block; margin-bottom: 8px; font-weight: bold; color: #555; }"
        "        input[type='text'], input[type='password'], input[type='number'] { width: calc(100% - 20px); padding: 10px; margin-bottom: 20px; border: 1px solid #ccc; border-radius: 4px; font-size: 16px; }"
        "        input[type='checkbox'] { margin-right: 10px; }"
        "        button { background-color: #007bff; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 18px; width: 100%; transition: background-color 0.3s ease; }"
        "        button:hover { background-color: #0056b3; }"
        "        .form-group { margin-bottom: 15px; }"
        "    </style>"
        "</head>"
        "<body>"
        "    <div class='container'>"
        "        <h1>WiFi and Device Configuration</h1>"
        "        <form action='/save_config' method='post'>"
        "            <div class='form-group'>"
        "                <label for='ssid'>WiFi SSID:</label>"
        "                <input type='text' id='ssid' name='ssid' placeholder='Your WiFi SSID' required>"
        "            </div>"
        "            <div class='form-group'>"
        "                <label for='password'>WiFi Password:</label>"
        "                <input type='password' id='password' name='password' placeholder='Your WiFi Password (min 8 chars)' minlength='8'>"
        "            </div>"
        "            <div class='form-group'>"
        "                <label for='hostname'>Device Hostname:</label>"
        "                <input type='text' id='hostname' name='hostname' placeholder='e.g., device' value='ESP32_CF_DEVICE'>"
        "            </div>"
        "            <div class='form-group'>"
        "                <label for='delay'>USB Remount Delay (ms):</label>"
        "                <input type='number' id='delay' name='delay' value='100' min='0' max='5000'>"
        "            </div>"
        "            <div class='form-group'>"
        "                <input type='checkbox' id='remount' name='remount' value='1' checked>"
        "                <label for='remount'>Restart USB after write operations</label>"
        "            </div>"
        "            <button type='submit'>Save Configuration and Restart</button>"
        "        </form>"
        "    </div>"
        "</body>"
        "</html>";
    return httpd_resp_send_chunk(req, html_form, strlen(html_form));
}

// Handler for saving config (POST /save_config)
static esp_err_t config_post_handler(httpd_req_t *req) {
    char *buf = malloc(req->content_len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for POST content");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        free(buf);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid_val[MAX_LEN + 1] = {0};
    char pwd_val[MAX_LEN + 1] = {0};
    char hostname_val[MAX_LEN + 1] = {0};
    uint8_t remount_flag = 0;
    int delay_val = 100;

    char *buf_copy = strdup(buf);
    if (!buf_copy) {
        ESP_LOGE(TAG, "Failed to duplicate buffer");
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    char *token = strtok(buf_copy, "&");
    while (token != NULL) {
        char *eq_pos = strchr(token, '=');
        if (eq_pos) {
            *eq_pos = '\0';
            char *key = token;
            char *value = eq_pos + 1;

            char decoded_value[MAX_LEN + 1];
            url_decode(value, decoded_value, sizeof(decoded_value));

            if (strcmp(key, "ssid") == 0) {
                strlcpy(ssid_val, decoded_value, sizeof(ssid_val));
            } else if (strcmp(key, "password") == 0) {
                strlcpy(pwd_val, decoded_value, sizeof(pwd_val));
            } else if (strcmp(key, "hostname") == 0) {
                strlcpy(hostname_val, decoded_value, sizeof(hostname_val));
            } else if (strcmp(key, "remount") == 0) {
                remount_flag = (strcmp(decoded_value, "1") == 0) ? 1 : 0;
            } else if (strcmp(key, "delay") == 0) {
                delay_val = atoi(decoded_value);
            }
        }
        token = strtok(NULL, "&");
    }
    free(buf);
    free(buf_copy);


    ESP_LOGI(TAG, "Saving config to NVS: SSID=%s, Hostname=%s, Remount=%d, Delay=%d",
             ssid_val, hostname_val, remount_flag, delay_val);

    if (save_config_to_nvs(ssid_val, pwd_val, hostname_val, remount_flag, delay_val) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save configuration to NVS.");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Configuration saved to NVS. Restarting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}


// Function to start the configuration HTTP server (for AP mode)
esp_err_t start_config_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 4096;
    config.max_uri_handlers = 4;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d' for configuration", config.server_port);
    if (httpd_start(&config_server_handle, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start config server.");
        return ESP_FAIL;
    }

    httpd_uri_t config_get = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = config_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(config_server_handle, &config_get);

    httpd_uri_t config_post = {
        .uri        = "/save_config",
        .method    = HTTP_POST,
        .handler   = config_post_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(config_server_handle, &config_post);

    ESP_LOGI(TAG, "Config HTTP server started.");
    return ESP_OK;
}