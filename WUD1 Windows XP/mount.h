#ifndef MOUNT_H
#define MOUNT_H

#include "sd_protocol_types.h"

#define SDMMC_PIN_CMD   14
#define SDMMC_PIN_CLK   21
#define SDMMC_PIN_D0    13 
#define SDMMC_PIN_D1    12
#define SDMMC_PIN_D2    10
#define SDMMC_PIN_D3    11

#define BASE_PATH "/sdcard"

esp_err_t mount_sdmmc(sdmmc_card_t **card);
esp_err_t umount_sdmmc(sdmmc_card_t *card);

#endif
