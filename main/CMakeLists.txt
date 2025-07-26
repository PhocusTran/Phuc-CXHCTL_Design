idf_component_register( SRCS "wifi_server.c"
                             "file_server.c"
                             "mount.c"
                             "usb_msc.c"
                             "lock.c"
                             "main.c" 
                        INCLUDE_DIRS "."
                        REQUIRES "esp_http_server" "esp_wifi" "esp_netif" "nvs_flash" "espressif__mdns" "espressif__esp_tinyusb" "sdmmc" "fatfs" "esp_http_client"
                        EMBED_FILES ./static/output.css ./static/script.js)
