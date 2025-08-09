#include "main.h"
#include "FreeRTOSConfig.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif_types.h"
#include "file_server.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "portmacro.h"
#include "wifi_server.h"
#include "sd_protocol_types.h"
#include "mount.h" // Cho mount_sdmmc và umount_sdmmc
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "tusb_msc_storage.h" // Vẫn cần vì sdmmc_card_t từ đây

#include "lock.h"
#include "usb_msc.h" // THÊM DÒNG NÀY ĐỂ CÓ CÁC PROTOTYPE CỦA usb_msc.c
#include "driver/gpio.h"
#include <inttypes.h>

#define SDIO_TO_CNC_GPIO_COUNT 6
#define SDIO_TO_ESP_GPIO_COUNT 6

int gpio_cnc[SDIO_TO_CNC_GPIO_COUNT] = {7, 8, 9, 35, 36, 37};
int gpio_esp[SDIO_TO_ESP_GPIO_COUNT] = {1, 2, 3, 4, 5, 6};

static const char* TAG_GPIO = "GPIO_SWITCH";

extern sdmmc_card_t *sd_card; // extern cho biến sd_card toàn cục
static const char* TAG = "MAIN";
char ip_str[16] = {0};

uint8_t op_mode = PARALEL_MODE; 
current_wifi_op_mode_t current_wifi_op_mode = WIFI_MODE_NONE;

void mode_switch_req_handler(void *param){
    ESP_LOGI(TAG, "mode_switch_req_handler triggered. Requested op_mode: %s", op_mode == HTTP_MODE ? "HTTP_MODE" : "USB_MODE");

    if (current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) {
        ESP_LOGW(TAG, "Cannot switch SD access mode while in AP_MODE. Please configure WiFi first.");
        op_mode = (op_mode == HTTP_MODE) ? USB_MODE : HTTP_MODE; // Toggle back if it was set
        return;
    }

    if(op_mode == USB_MODE){ // User wants USB MSC access (CNC/PC)
        ESP_LOGI(TAG, "Attempting to switch to USB Mode (CNC/PC SD Access).");
        // Deinitialize HTTP access (VFS) and initialize USB MSC
        my_tinyusb_msc_sdmmc_deinit(&sd_card); // Sẽ gọi umount và chuyển GPIO
        my_tinyusb_msc_sdmmc_init(&sd_card); // Sẽ init MSC và chuyển GPIO
        ESP_LOGI(TAG, "USB Mode Activated for CNC/PC.");

    } else { // User wants HTTP access (ESP32)
        ESP_LOGI(TAG, "Attempting to switch to WIFI Mode (ESP32 SD Access).");
        // Deinitialize USB MSC and initialize HTTP access (VFS)
        my_tinyusb_msc_sdmmc_deinit(&sd_card); // Sẽ deinit MSC và chuyển GPIO (về CNC)
        // Việc mount_sdmmc (kích hoạt GPIO ESP32) sẽ diễn ra trong các handler file khi cần.
        // Chỉ cần đảm bảo trạng thái GPIO cuối cùng là ESP32 có thể truy cập.
        switch_to_esp32(); // Kích hoạt GPIO cho ESP32
        ESP_LOGI(TAG, "WIFI Mode Activated for ESP32.");
    }
}

#define USB_FS_IDLE_TIMEOUT_MS 100
void usb_fs_monitor_task(void *arg) {
    while (true) {
        if (op_mode == HTTP_MODE && current_wifi_op_mode == WIFI_MODE_STA_ACTIVE) {
            if (usb_fs_busy && (xTaskGetTickCount() - last_usb_access_time > pdMS_TO_TICKS(USB_FS_IDLE_TIMEOUT_MS))) {
                usb_fs_busy = false;
                usb_msc_access_free();
            }
        }
        vTaskDelay(100);
    }
}

char ssid[MAX_LEN + 1] = {0};
char pwd[MAX_LEN + 1] = {0};
char hostname[MAX_LEN + 1] = "ESP32_CF_DEVICE";
int32_t delay = 100;
uint8_t restart_usb_after_write = 1;

bool read_config_from_nvs(void){
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return false;
    }

    size_t required_size;

    required_size = 0;
    nvs_get_str(nvs_handle, NVS_KEY_SSID, NULL, &required_size);
    if (required_size > 0 && required_size <= MAX_LEN + 1) {
        err = nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &required_size);
        if (err == ESP_OK) ESP_LOGI(TAG, "NVS: SSID = %s", ssid);
        else ESP_LOGW(TAG, "NVS: Failed to get SSID (%s)", esp_err_to_name(err));
    } else {
        ssid[0] = '\0';
    }

    required_size = 0;
    nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, NULL, &required_size);
    if (required_size > 0 && required_size <= MAX_LEN + 1) {
        err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, pwd, &required_size);
        if (err == ESP_OK) ESP_LOGI(TAG, "NVS: Password read (hidden)");
        else ESP_LOGW(TAG, "NVS: Failed to get Password (%s)", esp_err_to_name(err));
    } else {
        pwd[0] = '\0';
    }

    required_size = 0;
    nvs_get_str(nvs_handle, NVS_KEY_HOSTNAME, NULL, &required_size);
    if (required_size > 0 && required_size <= MAX_LEN + 1) {
        err = nvs_get_str(nvs_handle, NVS_KEY_HOSTNAME, hostname, &required_size);
        if (err == ESP_OK) ESP_LOGI(TAG, "NVS: Hostname = %s", hostname);
        else ESP_LOGW(TAG, "NVS: Failed to get Hostname (%s)", esp_err_to_name(err));
    } else {
        strlcpy(hostname, "ESP32_CF_DEVICE", MAX_LEN + 1);
        ESP_LOGW(TAG, "NVS: Hostname not found, using default: %s", hostname);
    }

    err = nvs_get_u8(nvs_handle, NVS_KEY_REMOUNT, &restart_usb_after_write);
    if (err != ESP_OK) {
        restart_usb_after_write = 1;
        ESP_LOGW(TAG, "NVS: Remount flag not found or error (%s), defaulting to %d", esp_err_to_name(err), restart_usb_after_write);
    } else {
        ESP_LOGI(TAG, "NVS: Restart USB after write = %d", restart_usb_after_write);
    }

    err = nvs_get_i32(nvs_handle, NVS_KEY_DELAY, &delay);
    if (err != ESP_OK) {
        delay = 100;
        ESP_LOGW(TAG, "NVS: Delay not found or error (%s), defaulting to %" PRId32, esp_err_to_name(err), delay);
    } else {
        ESP_LOGI(TAG, "NVS: USB remount delay = %" PRId32, delay);
    }

    nvs_close(nvs_handle);

    if (strlen(ssid) > 0 && strlen(pwd) >= 8) {
        return true;
    } else {
        ESP_LOGW(TAG, "WiFi credentials missing or invalid in NVS. Starting in AP mode.");
        return false;
    }
}

TaskHandle_t remount_task_handler = NULL;
void remount_usb_task(void *para){
    uint32_t ulNotifiedValue;
    BaseType_t status;
    while(1){
        status = xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);
        if(status == pdTRUE) 
        {
            if (current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) {
                ESP_LOGW(TAG, "Skipping USB remount: device is in AP_MODE.");
                continue;
            }
            if (op_mode == HTTP_MODE) { 
                tiny_usb_remount(); // Gọi hàm từ usb_msc.c
            } else {
                ESP_LOGW(TAG, "Skipping USB remount: device is in USB_MODE. Already handled by mode switch.");
            }
        }
    }
}


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_switch_init(); // Default: CNC has SD access
    usb_msc_fs_lock_init();
    mode_switch_req_cb_register(mode_switch_req_handler);

    bool config_exists = read_config_from_nvs();

    op_mode = PARALEL_MODE; 

    if (config_exists) {
        ESP_LOGI(TAG, "WiFi config found in NVS. Attempting to connect to STA.");
        wifi_init_sta(ssid, pwd);
    } else {
        ESP_LOGI(TAG, "No valid WiFi config found in NVS. Starting in AP mode for configuration.");
        wifi_init_ap();
    }

    BaseType_t result;
    result = xTaskCreate(remount_usb_task, "remount task", 10240, NULL, 5, &remount_task_handler);
    if (result == pdPASS) ESP_LOGI(TAG, "Created remount_usb_task\n");

    result = xTaskCreate(usb_fs_monitor_task, "usb monitor task", 2048, NULL, 5, NULL);
    if (result == pdPASS) ESP_LOGI(TAG, "Created usb_fs_monitor_task\n");
}

void gpio_switch_init(void) {
    for (int i = 0; i < SDIO_TO_CNC_GPIO_COUNT; i++) {
        gpio_reset_pin(gpio_cnc[i]);
        gpio_set_direction(gpio_cnc[i], GPIO_MODE_OUTPUT);
    }
    for (int i = 0; i < SDIO_TO_ESP_GPIO_COUNT; i++) {
        gpio_reset_pin(gpio_esp[i]);
        gpio_set_direction(gpio_esp[i], GPIO_MODE_OUTPUT);
    }

    ESP_LOGI(TAG_GPIO, "GPIO initialized. Setting CNC access as default initially (GPIOs for CNC ON).");
    switch_to_cnc(); // Default state: CNC has access to SD card
}


void switch_to_esp32(void) {
    ESP_LOGI(TAG_GPIO, "Switching SDIO access to ESP32...");
    for (int i = 0; i < SDIO_TO_CNC_GPIO_COUNT; i++) {
        gpio_set_level(gpio_cnc[i], 0);  // Disconnect CNC
        ESP_LOGI(TAG_GPIO, "GPIO %d OFF (CNC -> DISCONNECTED)", gpio_cnc[i]);
    }
    for (int i = 0; i < SDIO_TO_ESP_GPIO_COUNT; i++) {
        gpio_set_level(gpio_esp[i], 1);  // Connect ESP32
        ESP_LOGI(TAG_GPIO, "GPIO %d ON (ESP32 -> CONNECTED)", gpio_esp[i]);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); // Wait for stability
}


void switch_to_cnc(void) {
    ESP_LOGI(TAG_GPIO, "Switching SDIO access to CNC...");
    for (int i = 0; i < SDIO_TO_ESP_GPIO_COUNT; i++) {
        gpio_set_level(gpio_esp[i], 0); // Disconnect ESP32
        ESP_LOGI(TAG_GPIO, "GPIO %d OFF (ESP32 -> DISCONNECTED)", gpio_esp[i]);
    }

    for (int i = 0; i < SDIO_TO_CNC_GPIO_COUNT; i++) {
        gpio_set_level(gpio_cnc[i], 1); // Connect CNC
        ESP_LOGI(TAG_GPIO, "GPIO %d ON  (CNC -> CONNECTED)", gpio_cnc[i]);
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
}