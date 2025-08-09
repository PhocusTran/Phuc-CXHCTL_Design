#include "esp_log.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_default_configs.h"
#include "file_server.h" // For BASE_PATH
#include "main.h" // For switch_to_esp32 and switch_to_cnc

#include "mount.h"
#include <errno.h>

static const char* TAG = "SDMMC";
extern sdmmc_card_t *sd_card;

esp_err_t mount_sdmmc(sdmmc_card_t **card){
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_err_t ret = ESP_OK;

    // QUAN TRỌNG: GPIO đã được chuyển sang ESP32 BỞI mode_switch_req_handler HOẶC HTTP HANDLER TRƯỚC KHI GỌI mount_sdmmc.
    // Dòng này đã được xóa trước đó và nên tiếp tục bị xóa ở đây.
    // switch_to_esp32(); 
    // vTaskDelay(100 / portTICK_PERIOD_MS); 

    slot.width = 4;
    slot.cmd = SDMMC_PIN_CMD;
    slot.clk = SDMMC_PIN_CLK;
    slot.d0  = SDMMC_PIN_D0;
    slot.d1  = SDMMC_PIN_D1;
    slot.d2  = SDMMC_PIN_D2;
    slot.d3  = SDMMC_PIN_D3;
    
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    if (*card == NULL) {
        *card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
        if (*card == NULL) {
            ESP_LOGE(TAG, "Could not allocate sdmmc_card_t");
            // Không chuyển GPIO ở đây.
            return ESP_ERR_NO_MEM;
        }
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = FILE_BUFSIZE
    };

    ESP_LOGI(TAG, "Attempting to mount SD card at %s", BASE_PATH);
    ret = esp_vfs_fat_sdmmc_mount(BASE_PATH, &host, &slot, &mount_config, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mount vfs error %s (code: %d)", esp_err_to_name(ret), ret);
    } else {
        ESP_LOGI(TAG, "SD card mounted successfully at %s", BASE_PATH);
        sdmmc_card_print_info(stdout, *card);
        // Kiểm tra xem /sdcard có tồn tại không
        struct stat st;
        if (stat(BASE_PATH, &st) != 0) {
            ESP_LOGE(TAG, "Directory %s does not exist, errno: %d", BASE_PATH, errno);
        } else {
            ESP_LOGI(TAG, "Directory %s exists", BASE_PATH);
        }
        // Kiểm tra thử mở thư mục /sdcard
        DIR *dir = opendir(BASE_PATH);
        if (!dir) {
            ESP_LOGE(TAG, "Cannot open directory %s, errno: %d", BASE_PATH, errno);
        } else {
            ESP_LOGI(TAG, "Directory %s opened successfully", BASE_PATH);
            closedir(dir);
        }
    }

    // KHÔNG TỰ ĐỘNG switch_to_cnc() ở cuối hàm này. Quyền điều khiển GPIO được quản lý bởi hàm gọi.
    return ret;
}

esp_err_t umount_sdmmc(sdmmc_card_t *card){
    // QUAN TRỌNG: GPIO đã được chuyển sang ESP32 BỞI HTTP HANDLER HOẶC mode_switch_req_handler.
    // Dòng này đã được xóa và nên tiếp tục bị xóa ở đây.
    // switch_to_esp32();
    // vTaskDelay(100 / portTICK_PERIOD_MS);

    esp_err_t ret = ESP_OK;
    if (card != NULL) {
        ret = esp_vfs_fat_sdcard_unmount(BASE_PATH, card);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Unmount vfs error %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "Attempted to unmount NULL SD card handle.");
        ret = ESP_ERR_INVALID_ARG;
    }
    
    // KHÔNG TỰ ĐỘNG switch_to_cnc() ở cuối hàm này.
    return ret;
};