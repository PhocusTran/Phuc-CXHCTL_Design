#ifndef USB_MSC_H
#define USB_MSC_H

// Các include cần thiết cho khai báo của các hàm và struct
#include "esp_err.h"
#include "sd_protocol_types.h" // Cho sdmmc_card_t
#include "tinyusb.h"           // Cho các struct TinyUSB
#include "tusb_msc_storage.h"  // Cho các hàm MSC storage API

// Các định nghĩa macro/enum cho TinyUSB, an toàn khi đặt trong header
#define EPNUM_MSC       1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,

    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

#ifdef __cplusplus
extern "C" {
#endif

// Khai báo các biến và hằng toàn cục ĐƯỢC ĐỊNH NGHĨA trong usb_msc.c
// Chúng không phải là static trong usb_msc.c để có thể extern ở đây.
extern tusb_desc_device_t descriptor_config;
extern uint8_t const msc_fs_configuration_desc[];
extern const tinyusb_config_t tusb_cfg; // extern cho tusb_cfg
extern char const *string_desc_arr[]; // extern cho string_desc_arr (nếu dùng)

// Khai báo các hàm public (mà các file khác có thể gọi)
extern sdmmc_card_t *sd_card; // extern cho biến sd_card toàn cục
void my_tinyusb_msc_sdmmc_init(sdmmc_card_t **card);
void my_tinyusb_msc_sdmmc_deinit(sdmmc_card_t **card);
void tiny_usb_remount(void);

// Lưu ý: storage_init_sdmmc và storage_deinit_sdmmc sẽ là static trong usb_msc.c
// nên chúng sẽ không có prototype ở đây (không cần gọi từ bên ngoài).

#ifdef __cplusplus
}
#endif

#endif // USB_MSC_H