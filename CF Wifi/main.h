#ifndef MAIN_H
#define MAIN_H

#include <stdint.h> // Ensure uint8_t is defined

enum operation_mode {
    HTTP_MODE,
    USB_MODE,
    PARALEL_MODE
};

// Define a new enum for WiFi operation mode
typedef enum {
    WIFI_MODE_NONE,
    WIFI_MODE_STA_ACTIVE,
    WIFI_MODE_AP_ACTIVE
} current_wifi_op_mode_t;

void switch_to_cnc(void);
void switch_to_esp32(void);
void gpio_switch_init(void);

// Thêm định nghĩa MAX_LEN vào đây (nếu chưa có)
#define MAX_LEN 25

// Thêm các định nghĩa NVS partition name và keys cho cấu hình
#define NVS_NAMESPACE "config_data" // Tên namespace cho cấu hình
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASSWORD "password"
#define NVS_KEY_HOSTNAME "hostname"
#define NVS_KEY_REMOUNT "remount"
#define NVS_KEY_DELAY "delay"

#endif