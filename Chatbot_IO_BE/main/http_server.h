#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_event_base.h"
#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>

#include <mqtt_client.h>


esp_err_t ping_handler(httpd_req_t *req);
httpd_handle_t start_webserver(void);

#endif /* end of include guard: HTTP_SERVER_H */
