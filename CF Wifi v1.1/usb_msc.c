#include "usb_msc.h"
#include "esp_log.h"
#include "driver/sdmmc_host.h"
#include "freertos/idf_additions.h"
#include "mount.h" // Cho SDMMC_PIN_*, mount_sdmmc/umount_sdmmc (nếu dùng)
#include "main.h" // Cho switch_to_esp32, switch_to_cnc (các hàm GPIO)
#include "esp_check.h"
#include "sdmmc_cmd.h"

// --- Định nghĩa các biến và hằng toàn cục (không static) ---
sdmmc_card_t *sd_card = NULL;
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
bool host_init = false;

tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, 
    .idProduct = 0x1025,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const msc_fs_configuration_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  
    "HugeUSB",                      
    "ESP32S3 USB",               
    "",                         
    "That USB MSC",                 
};

const tinyusb_config_t tusb_cfg = {
    .device_descriptor = &descriptor_config,
    .string_descriptor = string_desc_arr,
    .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
    .external_phy = false,
    .configuration_descriptor = msc_fs_configuration_desc,
};


static const char* TAG = "USB";

extern int32_t delay;

/* --- PROTOTYPE CÁC HÀM INTERNAL STATIC --- */
static esp_err_t storage_init_sdmmc(sdmmc_card_t **card);
static void storage_deinit_sdmmc(sdmmc_card_t **card);


/* --- ĐỊNH NGHĨA CÁC HÀM --- */

void storage_mount_changed_cb(tinyusb_msc_event_t *event) {
    printf("Storage mounted to application: %s\n", event->mount_changed_data.is_mounted ? "Yes" : "No");
}

static esp_err_t storage_init_sdmmc(sdmmc_card_t **card)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Initializing SDCard for USB MSC");

    slot_config.width = 4;
    slot_config.cmd = SDMMC_PIN_CMD; 
    slot_config.clk = SDMMC_PIN_CLK;
    slot_config.d0  = SDMMC_PIN_D0;
    slot_config.d1  = SDMMC_PIN_D1;
    slot_config.d2  = SDMMC_PIN_D2;
    slot_config.d3  = SDMMC_PIN_D3;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    if (*card == NULL) {
        *card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
        ESP_GOTO_ON_FALSE(*card, ESP_ERR_NO_MEM, clean, TAG, "could not allocate new sdmmc_card_t");
    }

    ESP_GOTO_ON_ERROR((*host.init)(), clean, TAG, "Host Config Init fail");
    host_init = true;

    ESP_GOTO_ON_ERROR(sdmmc_host_init_slot(host.slot, (const sdmmc_slot_config_t *) &slot_config),
                      clean, TAG, "Host init slot fail");

    // QUAN TRỌNG: Không gọi switch_to_esp32() ở đây nữa. Nó được quản lý bên ngoài.
    // VFS init sẽ cần ESP32 quyền truy cập. MSC init cần ESP32 quyền truy cập tạm thời.
    // Quyết định cuối cùng của GPIO nằm ở my_tinyusb_msc_sdmmc_init.
    // DÒNG NÀY ĐÃ ĐƯỢC XÓA: switch_to_esp32();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    while (sdmmc_card_init(&host, *card)) {
        ESP_LOGE(TAG, "The detection pin of the slot is disconnected(Insert uSD card). Retrying...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    
    // QUAN TRỌNG: Không gọi switch_to_cnc() ở đây nữa. Quyết định cuối cùng nằm ở my_tinyusb_msc_sdmmc_init.
    // DÒNG NÀY ĐÃ ĐƯỢC XÓA: switch_to_cnc();

    sdmmc_card_print_info(stdout, *card);
    printf("init pointer %p\n\r", *card);

    return ESP_OK;

clean:
    if (host_init) {
        if (host.flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
            host.deinit_p(host.slot);
        } else {
            (*host.deinit)();
        }
        host_init = false;
    }
    if (*card) {
        free(*card);
        *card = NULL;
    }
    // Không gọi switch_to_cnc() ở đây.
    return ret;
}

static void storage_deinit_sdmmc(sdmmc_card_t **card){
    // QUAN TRỌNG: Không gọi switch_to_esp32() ở đây nữa.
    // DÒNG NÀY ĐÃ ĐƯỢC XÓA: switch_to_esp32();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    if (host_init) {
        if (host.flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
            host.deinit_p(host.slot);
        } else {
            (*host.deinit)();
        }
        host_init = false;
    }
    // QUAN TRỌNG: Không gọi switch_to_cnc() ở đây nữa.
    // DÒNG NÀY ĐÃ ĐƯỢC XÓA: switch_to_cnc();
}

void my_tinyusb_msc_sdmmc_deinit(sdmmc_card_t **card){
    // Khi deinit USB MSC, GPIO phải thuộc về ESP32 để deinit sạch sẽ
    switch_to_esp32();
    tinyusb_driver_uninstall();
    tinyusb_msc_storage_deinit();
    storage_deinit_sdmmc(card); // Gọi hàm static internal (sẽ không chuyển GPIO)
    switch_to_cnc(); // Sau khi deinit xong, trả quyền cho CNC
}

void tiny_usb_remount(){
    ESP_LOGI(TAG, "Attempting USB remount...");
    switch_to_esp32(); // GPIO phải thuộc về ESP32 để TinyUSB thực hiện các thao tác internal
    vTaskDelay(pdMS_TO_TICKS(delay));
    tinyusb_driver_uninstall();
    
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB remount DONE");
    switch_to_cnc(); // Sau khi remount hoàn tất, trả lại GPIO cho CNC
}

void my_tinyusb_msc_sdmmc_init(sdmmc_card_t **card){
    // Đây là hàm public init cho USB MSC.
    // GPIO ban đầu phải thuộc về ESP32 để thực hiện init host/card.
    switch_to_esp32(); // DÒNG NÀY ĐÚNG

    if (!host_init || *card == NULL) { 
        ESP_ERROR_CHECK(storage_init_sdmmc(card)); // storage_init_sdmmc sẽ KHÔNG thay đổi GPIO cuối cùng
    } else {
        ESP_LOGI(TAG, "SD Card host already initialized for USB MSC. Proceeding.");
    }

    const tinyusb_msc_sdmmc_config_t config_sdmmc = (tinyusb_msc_sdmmc_config_t){
        .card = *card,
        .callback_mount_changed = storage_mount_changed_cb,  
        .mount_config.max_files = 5,
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&config_sdmmc));

    ESP_LOGI(TAG, "USB MSC initialization");

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB MSC initialization DONE");

    // RẤT QUAN TRỌNG: Sau khi toàn bộ USB MSC được khởi tạo, trả quyền cho CNC.
    // USB MSC sẽ làm việc qua CNC.
    switch_to_cnc(); // DÒNG NÀY ĐÚNG
}