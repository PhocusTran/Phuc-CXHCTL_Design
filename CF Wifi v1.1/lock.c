#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "lock.h"
#include <stdbool.h>
#include <stdint.h>

static const char* TAG = "SEMA";

uint8_t usb_seize_request = 0;
uint8_t http_busy = 0;

SemaphoreHandle_t fs_access_sema = NULL;
volatile bool usb_taken_assess = false;

void usb_msc_fs_lock_init() {
    if (!fs_access_sema) {
        fs_access_sema = xSemaphoreCreateBinary();
        assert(fs_access_sema != NULL);
        ESP_LOGW(TAG, "USB lock Init");
        // xSemaphoreGive(fs_access_sema);
    }
    ESP_LOGW(TAG, "Inited");
}

void usb_msc_fs_lock(void) {
    if (fs_access_sema) {
        xSemaphoreTake(fs_access_sema, portMAX_DELAY);
    }
}

void usb_msc_fs_unlock(void) {
    if (fs_access_sema) {
        xSemaphoreGive(fs_access_sema);
    }
}

void usb_msc_access_lock(void){
    if(!usb_taken_assess){
        // usb_msc_fs_lock();
        usb_taken_assess = true;
    }
}

void usb_msc_access_free(void){
    if(usb_taken_assess){
        usb_taken_assess = false;
    }
}

uint8_t end_http_task(void){
    if(http_busy==0) return 1;

    usb_seize_request = 1;

    ESP_LOGW(TAG, "Set busy %d", usb_seize_request);
    while(http_busy==1) vTaskDelay(1);
    ESP_LOGW(TAG, "Not anymore %d", http_busy);
    return 1;
}