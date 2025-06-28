#include "http_server.h"

esp_err_t ping_handler(httpd_req_t *req) {
    const char *response = "Pong";
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t ping_uri = {
            .uri       = "/ping",
            .method    = HTTP_GET,
            .handler   = ping_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &ping_uri);
    }

    return server;
}

