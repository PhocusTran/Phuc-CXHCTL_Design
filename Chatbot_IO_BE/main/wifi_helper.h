#ifndef WIFI_HELPER_H
#define WIFI_HELPER_H

#include "esp_event_base.h"
#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>
#include "extra_event_callback.h"

void wifi_init(char *ssid, char *pwd);

// event callbackfor wifi event
// ** only has event_data since IP is what mostly needed

void extra_wifi_event_register(event_callback_t callback);

#endif /* end of include guard: WIFI_HELPER_H */
