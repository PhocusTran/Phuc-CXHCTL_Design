#ifndef NVS_HELPER_H
#define NVS_HELPER_H
#include "nvs_flash.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs.h"

void init_nvs();
void write_to_nvs(const char *key, int value);
void write_string_to_nvs(const char *key, const char *value);
void read_string_from_nvs(const char *key, char *buffer, size_t buffer_size);
int read_from_nvs(const char *key);

#endif /* end of include guard: NVS_HELPER_H */
