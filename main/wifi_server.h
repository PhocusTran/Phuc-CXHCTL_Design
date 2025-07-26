#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "main.h" // Include main.h for current_wifi_op_mode_t and NVS_NAMESPACE/KEYS

esp_err_t start_file_server(void); // Corrected declaration (should be esp_err_t, not httpd_handle_t)

esp_err_t wifi_init_sta(const char* ssid, const char* pwd);
esp_err_t wifi_init_ap(void);
esp_err_t start_config_server(void); // New function for AP mode config server

// Khai báo hàm mới để lưu cấu hình vào NVS
esp_err_t save_config_to_nvs(const char *ssid, const char *pwd, const char *hostname_val, uint8_t remount_flag, int delay_val);

extern current_wifi_op_mode_t current_wifi_op_mode; // To track current WiFi mode

#endif