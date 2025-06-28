#include "nvs_helper.h"

void init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
}

/* 
void write_to_nvs(const char *key, int value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS!", esp_err_to_name(err));
        return;
    }

    ESP_LOGI("NVS", "Writing %s = %d", key, value);
    err = nvs_set_i32(nvs_handle, key, value);

    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to write!");
    }

    nvs_commit(nvs_handle);  // Make sure data is actually saved
    nvs_close(nvs_handle);
}
*/

void write_string_to_nvs(const char *key, const char *value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS!", esp_err_to_name(err));
        return;
    }

    ESP_LOGI("NVS", "Writing %s = %s", key, value);
    err = nvs_set_str(nvs_handle, key, value);

    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to write string!");
    }

    nvs_commit(nvs_handle);  // Ensure data is saved
    nvs_close(nvs_handle);
}

void read_string_from_nvs(const char *key, char *buffer, size_t buffer_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS!", esp_err_to_name(err));
        return;
    }

    size_t length = buffer_size;
    err = nvs_get_str(nvs_handle, key, buffer, &length);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "String not found");
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) reading string!", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

/*
int read_from_nvs(const char *key) {
    nvs_handle_t nvs_handle;
    int value = 0;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) opening NVS!", esp_err_to_name(err));
        return -1;
    }

    err = nvs_get_i32(nvs_handle, key, (int32_t*)&value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "Value not found");
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Error (%s) reading!", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return value;
}
 */
