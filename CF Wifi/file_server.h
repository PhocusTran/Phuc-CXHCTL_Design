#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#include "esp_err.h"
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "esp_vfs.h"
#include "extran_event_callback.h"
#include "main.h" // For current_wifi_op_mode_t and MAX_LEN

#define FILE_BUFSIZE  64 * 1024
#define MAX_FILE_PATH 256
#define BASE_PATH "/sdcard" // Keep this for SD card VFS operations

struct file_server_buffer {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char buffer[FILE_BUFSIZE];
};

esp_err_t start_file_server(void);
void mode_switch_req_cb_register(event_callback_t callback);
esp_err_t url_decode(const char *input, char *output, int output_len); // Make public for wifi_server.c

// Xóa khai báo này vì hàm này sẽ được di chuyển và đổi tên
// esp_err_t save_config_file(const char *ssid, const char *pwd, const char *hostname_val, uint8_t remount_flag, int delay_val);

extern current_wifi_op_mode_t current_wifi_op_mode; // To check WiFi mode

#endif