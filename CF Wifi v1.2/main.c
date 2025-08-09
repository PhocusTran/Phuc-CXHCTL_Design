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
#include "portmacro.h"
#include "wifi_server.h"
#include "sd_protocol_types.h"
#include "mount.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "tusb_msc_storage.h"

#include "lock.h"
#include "usb_msc.h"

sdmmc_card_t *sd_card = NULL;
static const char* TAG = "MAIN";
char ip_str[16] = {0};

uint8_t op_mode = PARALEL_MODE;


void mode_switch_req_handler(void *param){
    op_mode = !op_mode;

    ESP_LOGI(TAG, "Current operate mode %s", op_mode ? "USB_MODE" : "HTTP_MODE");

    if(op_mode == USB_MODE){
        ESP_LOGI(TAG, "Unmount %s", !umount_sdmmc(sd_card)? "OK" : "Fail");
        my_tinyusb_msc_sdmmc_init(&sd_card);
        ESP_LOGI(TAG, "SD_CARD %s", sd_card == NULL ? "NULL" : "NOT NULL");
    } else {
        my_tinyusb_msc_sdmmc_deinit(&sd_card);
        ESP_LOGI(TAG, "Remount %s", !mount_sdmmc(&sd_card)? "OK" : "Fail");
    }
}

#define USB_FS_IDLE_TIMEOUT_MS 100
void usb_fs_monitor_task(void *arg) {
    while (true) {
        if (usb_fs_busy && (xTaskGetTickCount() - last_usb_access_time > pdMS_TO_TICKS(USB_FS_IDLE_TIMEOUT_MS))) {
            usb_fs_busy = false;
            usb_msc_access_free();
        }
        vTaskDelay(100);
    }
}

#define MAX_LEN 25
// Helper function to trim leading and trailing whitespace
void trim(char *str) {
    // Trim leading space
    char *start = str;
    while (isspace((unsigned char)*start)) start++;

    // Move the trimmed string to the front
    if (start != str) memmove(str, start, strlen(start) + 1);

    // Trim trailing space
    char *end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) *end-- = '\0';
}

char ssid[25] = {0};
char pwd[25] = {0};
char hostname[25] = "USB_WIFI";
int delay = 100;

uint8_t restart_usb_after_write = 0;
void parse_line(const char *buf) {
    if (strncmp("ssid", buf, 4) == 0) {
        strncpy(ssid, buf + 5, MAX_LEN - 1);
        trim(ssid);
        printf("SSID: |%s|\n", ssid);
    } else if (strncmp("password", buf, 8) == 0) {
        strncpy(pwd, buf + 9, MAX_LEN - 1);
        trim(pwd);
        printf("PWD: |%s|\n", pwd);
    } else if (strncmp("hostname", buf, 8) == 0) {
        strncpy(hostname, buf + 9, MAX_LEN - 1);
        trim(hostname);
        printf("hostname: |%s|\n", hostname);
    } else if (strncmp("remount", buf, 7) == 0){
        if(buf[8] == '1'){
            restart_usb_after_write = 1;
        } else if(buf[8] == '0'){
            restart_usb_after_write = 0;
        }
    } else if (strncmp("delay", buf, 5) == 0){
        char delay_str[10] = {0};
        strncpy(delay_str, buf + 6, 9);
        trim(delay_str);
        delay = atoi(delay_str);
        printf("delay: |%d|\n", delay);
    }
}


uint8_t read_wifi_from_dot_file(){
    char* filepath = BASE_PATH  "/.config";
    
    FILE *ptr = fopen(filepath, "r");
    if (ptr == NULL) {
        ESP_LOGE(TAG, "Filename not present - %s", filepath);
        return ESP_FAIL;
    }
    char buf[254];
    int count = 0;
    while (fgets(buf, 254, ptr) != NULL) {
        if(count >= 5) break;
        parse_line(buf);
        count++;
    }
    fclose(ptr);
    return ESP_OK;
}

TaskHandle_t remount_task_handler = NULL;
void remount_usb_task(void *para){
    uint32_t ulNotifiedValue;
    BaseType_t status;
    while(1){
        status = xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);
        if(status == pdTRUE) 
        {
            tiny_usb_remount();
        }
    }
}


void app_main(void)
{
    usb_msc_fs_lock_init();
    mode_switch_req_cb_register(mode_switch_req_handler);

    mount_sdmmc(&sd_card);

    my_tinyusb_msc_sdmmc_init(&sd_card);

    if(read_wifi_from_dot_file() == ESP_OK){
        if((ssid[0]!='\0') && (pwd[0]!='\0')) wifi_init(ssid, pwd);
    }

    BaseType_t result;
    result = xTaskCreate(remount_usb_task, "remount task", 10240, NULL, 5, &remount_task_handler);
    if (result == pdPASS) printf("Created remount_usb_task\n");

    xTaskCreate(usb_fs_monitor_task, "usb monitor task", 1024, NULL, 5, NULL);
}
