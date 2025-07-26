#include "esp_compiler.h"
#include "esp_err.h"
#include "esp_log.h"
#include "file_server.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_http_server.h"
#include "freertos/idf_additions.h"
#include "http_parser.h"
#include "lock.h"
#include "main.h" // Chứa op_mode, current_wifi_op_mode_t, hostname, MAX_LEN
#include "task.h"
#include "sys/dirent.h"
#include "mount.h"

#include "tusb_msc_storage.h"
#include "extran_event_callback.h"
#include "usb_msc.h"
#include "esp_http_client.h"
#include "sdkconfig.h"
#include "sys/param.h"

// Định nghĩa macro
#define MAX_URI_AND_NAME_LEN (CONFIG_HTTPD_MAX_URI_LEN + NAME_MAX + 10)

// Các biến extern (được định nghĩa ở file khác, ví dụ main.c)
static const char* TAG = "FILE";
extern uint8_t op_mode;
extern uint8_t restart_usb_after_write;
extern TaskHandle_t remount_task_handler;
extern char hostname[25]; 
extern current_wifi_op_mode_t current_wifi_op_mode;
extern sdmmc_card_t *sd_card; // Khai báo extern cho biến toàn cục sd_card

extra_event_callback mode_switch_req_cb = {0};

/* --- CÁC BIẾN TOÀN CỤC CỐ ĐỊNH (CONST GLOBAL VARIABLES) --- */
const char *deletemenu = "<div id=\"context-menu\" class=\"hidden fixed z-50 bg-white border rounded shadow-md w-32 text-sm\"> <form method=\"post\" id=\"delete-form\"> <button type=\"submit\" class=\"w-full text-left px-4 py-2 hover:bg-red-100 text-red-600\">Delete</button> </form> </div>\r\n";

const char *head1 =
    "<!doctype html>\n<html lang=\"en\">\n    <head>\n        <meta charset=\"UTF-8\" />\n        <title>";

const char *head2 = "</title>\n        <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\" />\n        <meta name=\"description\" content=\"\" />\n        <link rel=\"icon\" href=\"favicon.png\" />\n <link href=\"/output.css\" rel=\"stylesheet\"> \n    </head>\n <body>";

const char *upload_and_mode_control =
    "        <div class=\"space-y-3 rounded-md border border-gray-300 p-4 shadow-sm\">\n"
"            <h2 class=\"text-lg font-semibold\">Upload file</h2>\n"
"\n"
"            <input id=\"newfile\" type=\"file\" multiple onchange=\"setpath()\" class=\"block w-full text-sm file:mr-4 file:rounded file:border-0 file:bg-sky-100 file:p-2 file:text-md file:font-semibold hover:file:bg-sky-200\" />\n"
"\n"
"            <div class=\"flex items-center space-x-2\">\n"
"                <input id=\"filepath\" type=\"text\" placeholder=\"Set path on server\" class=\"w-1/2 rounded border border-gray-300 px-3 py-2 focus:ring-2 focus:ring-sky-300 focus:outline-none\" />\n"
"                <button id=\"upload_btn\" type=\"button\" onclick=\"upload()\" class=\"rounded border border-sky-600 text-sky-600 px-4 py-2 hover:bg-sky-100\">Upload</button>\n"
"                <button id=\"remount_btn\" type=\"button\" onclick=\"remount()\" class=\"rounded border border-yellow-600 text-yellow-600 px-4 py-2 hover:bg-yellow-100\">Remount USB</button>\n"
"                <button id=\"delete_selected_btn\" type=\"button\" onclick=\"deleteSelected()\" class=\"rounded border border-red-600 text-red-600 px-4 py-2 hover:bg-red-100\">Delete Selected</button>\n"
"            </div>\n"
"        </div>\n"
"        <div class=\"space-y-3 rounded-md border border-gray-300 p-4 shadow-sm mt-4\">\n" // New div for mode control
"            <h2 class=\"text-lg font-semibold\">Device Mode Control</h2>\n"
"            <div class=\"flex items-center space-x-2\">\n"
"                <button type=\"button\" onclick=\"switchToWifiMode()\" class=\"rounded border %s px-4 py-2\">Access SD via WiFi (ESP32)</button>\n"
"                <button type=\"button\" onclick=\"switchToUsbMode()\" class=\"rounded border %s px-4 py-2\">Access SD via USB (CNC/PC)</button>\n"
"            </div>\n"
"        </div>\n";

/* --- KHAI BÁO HÀM (FUNCTION PROTOTYPES) --- */
#ifdef __cplusplus
extern "C" {
#endif

// Helper functions
static bool is_editable(const char *filename);
static const char* get_path_from_uri(char* dest, const char* base_path ,const char* uri, size_t dest_size);
int fix_space(char* dest, const char* src);
esp_err_t set_content_type_from_file(httpd_req_t *r, const char* filename);
esp_err_t url_decode(const char *input, char *output, int output_len);

// Callback registration
void mode_switch_req_cb_register(event_callback_t callback);
void trigger_mode_switch_req_cb(void *event_data);

// HTTP Server response helpers
esp_err_t index_redirect_handler(httpd_req_t *r);
esp_err_t server_busy_response(httpd_req_t *r, const char* message);

// HTTP Server URI handlers
esp_err_t mode_selection_page_handler(httpd_req_t *r);
esp_err_t mode_selection_post_handler(httpd_req_t *r);
esp_err_t esp_resp_dir_html(httpd_req_t *r, const char* file_path);
esp_err_t download_handler(httpd_req_t *r);
esp_err_t edit_handler(httpd_req_t *r);
esp_err_t save_handler(httpd_req_t *r);
esp_err_t file_upload_handler(httpd_req_t *r);
esp_err_t file_delete_handler(httpd_req_t *r);
esp_err_t remount_handler(httpd_req_t *r);
esp_err_t ping_get_handler(httpd_req_t *req);
esp_err_t css_get_handler(httpd_req_t *req);
esp_err_t script_get_handler(httpd_req_t *req);
esp_err_t device_mode_switch_handler(httpd_req_t *r);

#ifdef __cplusplus
}
#endif
/* --- KẾT THÚC KHAI BÁO HÀM (END FUNCTION PROTOTYPES) --- */

/* --- ĐỊNH NGHĨA CÁC HÀM (FUNCTION DEFINITIONS) --- */

// Các hàm Callback
void mode_switch_req_cb_register(event_callback_t callback){
    if(mode_switch_req_cb.callback_registered_count < MAX_CALLBACK){
        mode_switch_req_cb.callback[mode_switch_req_cb.callback_registered_count++] = callback;
    }
    else{
        ESP_LOGE(TAG, "callback reach limit");
    }
}

void trigger_mode_switch_req_cb(void *event_data){
    if(mode_switch_req_cb.callback_registered_count == 0) return;

    for (int i = 0; i < mode_switch_req_cb.callback_registered_count; i++){
        mode_switch_req_cb.callback[i](event_data);
    }
}

// Helper functions
static bool is_editable(const char *filename) {
    const char *editable_extensions[] = {
        ".txt", ".c", ".nc", ".h", ".html", ".js", ".css", ".json", ".md", ".log", ".config", ".py", ".cpp", ".err"
    };
    for (int i = 0; i < sizeof(editable_extensions) / sizeof(editable_extensions[0]); i++) {
        const char *dot = strrchr(filename, '.');
        if (dot && strcasecmp(dot, editable_extensions[i]) == 0) {
            return true;
        }
    }
    if (strcmp(filename, ".config") == 0) return true;
    
    return false;
}

static const char* get_path_from_uri(char* dest, const char* base_path ,const char* uri, size_t dest_size){
    const size_t base_path_len = strlen(base_path);
    size_t uri_len = strlen(uri);

    const char* quest = strchr(uri, '?');
    if(quest) uri_len = MIN(uri_len, quest - uri);

    const char* hash = strchr(uri, '#');
    if(hash) uri_len = MIN(uri_len, hash - uri);

    if(base_path_len + uri_len + 1 > dest_size) return NULL;

    strcpy(dest, base_path);
    strlcpy(dest + base_path_len, uri, uri_len + 1);

    return dest + base_path_len;
}

int fix_space(char* dest, const char* src){
    printf("%s %s\n", dest, src);
    char* p = strchr(src, '%');
    if(!p || (strlen(src) > MAX_FILE_PATH) || !(p[1]=='2') || !(p[2]=='0')) {
        strcpy(dest, src);
        return 1;
    }

    strncpy(dest, src, p - src);
    dest[p-src] = '\0';
    strcat(dest, " ");

    p = &p[3];

    while(1){
        char* _p = strchr(p, '%');
        if(_p) {
            strncat(dest, p, _p - p);
            strcat(dest, " ");
            p = &_p[3];
        }
        else{
            strcat(dest, p);
            break;
        }
    }
    printf("%s\n", dest);
    return 0;
}

esp_err_t set_content_type_from_file(httpd_req_t *r, const char* filename){
    const char *dot = strrchr(filename, '.');
    if (dot) {
        if (strcasecmp(dot, ".pdf") == 0) {
            return httpd_resp_set_type(r, "application/pdf");
        } else if (strcasecmp(dot, ".html") == 0) {
            return httpd_resp_set_type(r, "text/html");
        } else if (strcasecmp(dot, ".jpeg") == 0) {
            return httpd_resp_set_type(r, "image/jpeg");
        } else if (strcasecmp(dot, ".jpg") == 0) {
            return httpd_resp_set_type(r, "image/jpg");
        } else if (strcasecmp(dot, ".png") == 0) {
            return httpd_resp_set_type(r, "image/png");
        }
    }
    return httpd_resp_set_type(r, "text/plain");
}

esp_err_t url_decode(const char *input, char *output, int output_len) {
    char *out_ptr = output;
    const char *in_ptr = input;
    int decoded_len = 0;

    while (*in_ptr && decoded_len < output_len - 1) {
        if (*in_ptr == '%') {
            if (in_ptr[1] && in_ptr[2]) {
                char hex[3] = {in_ptr[1], in_ptr[2], '\0'};
                *out_ptr++ = strtol(hex, NULL, 16);
                in_ptr += 3;
            } else {
                *out_ptr++ = *in_ptr++;
                decoded_len++;
            }
        } else if (*in_ptr == '+') {
            *out_ptr++ = ' ';
            in_ptr++;
        } else {
            *out_ptr++ = *in_ptr++;
        }
        decoded_len++;
    }
    *out_ptr = '\0';
    return ESP_OK;
}


// HTTP Server response helpers
esp_err_t index_redirect_handler(httpd_req_t *r){
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "/");
    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_send(r, NULL, 0);
    return ESP_OK;
}

esp_err_t server_busy_response(httpd_req_t *r, const char* message) {
    if (current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) {
        httpd_resp_set_status(r, "302 Found");
        httpd_resp_set_hdr(r, "Location", "http://192.168.4.1/"); // Redirect to config page
        httpd_resp_send(r, NULL, 0);
        return ESP_FAIL;
    }
    
    httpd_resp_sendstr_chunk(r, "<!DOCTYPE html><html><body>");
    httpd_resp_sendstr_chunk(r, "<h2>");
    httpd_resp_sendstr_chunk(r, message);
    httpd_resp_sendstr_chunk(r, "</h2>");
    httpd_resp_sendstr_chunk(r, "</body></html>");
    httpd_resp_send_chunk(r, NULL, 0);
    return ESP_FAIL;
}


// HTTP Server URI handlers

// Handler cho trang chọn chế độ ban đầu (URI: /)
esp_err_t mode_selection_page_handler(httpd_req_t *r) {
    if (current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) {
        return server_busy_response(r, "Device is in AP Configuration mode. Please go to 192.168.4.1 to configure WiFi.");
    }

    // Nếu đã ở chế độ HTTP_MODE (ESP32 có thể truy cập file), chuyển hướng đến giao diện file server
    if (op_mode == HTTP_MODE) {
        httpd_resp_set_status(r, "302 Found");
        httpd_resp_set_hdr(r, "Location", "/fs"); // Redirect to file server interface
        httpd_resp_send(r, NULL, 0);
        return ESP_OK;
    } 
    // Nếu đã ở chế độ USB_MODE, hiển thị thông báo USB Mode Activated
    else if (op_mode == USB_MODE) {
        const char *response_html = 
            "<!DOCTYPE html><html><head><title>USB Mode Active</title>"
            "<style>body { font-family: sans-serif; text-align: center; margin-top: 50px; }"
            "button { padding: 10px 20px; font-size: 16px; cursor: pointer; background-color: #007bff; color: white; border: none; border-radius: 5px; }"
            "button:hover { background-color: #0056b3; }</style>"
            "</head><body>"
            "<h2>Device is currently in USB Mode. File server functions are disabled.</h2>"
            "<p>To access files via WiFi, please switch modes.</p>"
            "<form action='/select_mode' method='POST'>"
            "<input type='hidden' name='mode' value='wifi'>"
            "<button type='submit'>Switch to WiFi Mode</button>"
            "</form>"
            "</body></html>";
        httpd_resp_set_type(r, "text/html");
        httpd_resp_send(r, response_html, strlen(response_html));
        return ESP_OK;
    }

    // Nếu op_mode là PARALEL_MODE (trạng thái khởi tạo hoặc chưa xác định) hoặc cần chọn lần đầu
    const char *html = 
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>Select Mode</title>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\" />"
        "<style>"
        "body { font-family: sans-serif; text-align: center; padding: 50px; background-color: #f0f2f5; }"
        ".container { background-color: #fff; padding: 30px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); display: inline-block; }"
        "h1 { color: #333; margin-bottom: 30px; }"
        "button { padding: 15px 30px; margin: 10px; font-size: 18px; cursor: pointer; border: none; border-radius: 8px; transition: background-color 0.3s, transform 0.2s; }"
        ".wifi-btn { background-color: #4CAF50; color: white; }"
        ".wifi-btn:hover { background-color: #45a049; transform: translateY(-2px); }"
        ".usb-btn { background-color: #2196F3; color: white; }"
        ".usb-btn:hover { background-color: #0b7dda; transform: translateY(-2px); }"
        "</style>"
        "</head>"
        "<body>"
        "    <div class=\"container\">"
        "        <h1>Select Device Operation Mode</h1>"
        "        <p>Choose how you want to access the SD card:</p>"
        "        <form action='/select_mode' method='POST' style='display:inline-block;'>"
        "            <input type='hidden' name='mode' value='wifi'>"
        "            <button type='submit' class='wifi-btn'>Access SD via WiFi (ESP32)</button>"
        "        </form>"
        "        <form action='/select_mode' method='POST' style='display:inline-block;'>"
        "            <input type='hidden' name='mode' value='usb'>"
        "            <button type='submit' class='usb-btn'>Access SD via USB (CNC/PC)</button>"
        "        </form>"
        "    </div>"
        "</body>"
        "</html>";
    httpd_resp_set_type(r, "text/html");
    httpd_resp_send(r, html, strlen(html));
    return ESP_OK;
}

// Handler cho POST request từ trang chọn chế độ ban đầu
esp_err_t mode_selection_post_handler(httpd_req_t *r) {
    if (current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) {
        return server_busy_response(r, "Device is in AP Configuration mode. Cannot switch mode.");
    }
    if (usb_fs_busy) {
        return server_busy_response(r, "USB file system is busy. Cannot switch mode.");
    }

    char content_type[100];
    if (httpd_req_get_hdr_value_str(r, "Content-Type", content_type, sizeof(content_type)) != ESP_OK ||
        strstr(content_type, "application/x-www-form-urlencoded") == NULL) {
        ESP_LOGE(TAG, "Invalid Content-Type for mode select");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid Content-Type");
        return ESP_FAIL;
    }

    char buf[64];
    int ret = httpd_req_recv(r, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(r);
        } else {
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request data");
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char mode_str[16];
    if (httpd_query_key_value(buf, "mode", mode_str, sizeof(mode_str)) != ESP_OK) {
        ESP_LOGE(TAG, "Mode parameter not found");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Mode parameter missing");
        return ESP_FAIL;
    }

    // Cập nhật op_mode và kích hoạt callback để thực hiện chuyển đổi GPIO/USB MSC
    if (strcmp(mode_str, "wifi") == 0) {
        op_mode = HTTP_MODE;
        ESP_LOGI(TAG, "Mode selected: WIFI_MODE (ESP32 SD Access)");
        trigger_mode_switch_req_cb(NULL); // Kích hoạt chuyển đổi GPIO cho ESP32
        
        httpd_resp_set_status(r, "302 Found");
        httpd_resp_set_hdr(r, "Location", "/fs"); // Chuyển hướng đến giao diện file server
        httpd_resp_send(r, NULL, 0);
        return ESP_OK;
    } else if (strcmp(mode_str, "usb") == 0) {
        op_mode = USB_MODE;
        ESP_LOGI(TAG, "Mode selected: USB_MODE (CNC/PC SD Access)");
        trigger_mode_switch_req_cb(NULL); // Kích hoạt chuyển đổi GPIO cho USB MSC
        
        // Sau khi chuyển sang USB mode, không thể truy cập file server qua WiFi.
        // Hiển thị thông báo và cho phép quay lại trang chọn mode.
        const char *response_html = "<h3>USB Mode Activated</h3><p>To switch back, access <a href='/'>mode selection page</a>.</p>";
        httpd_resp_set_type(r, "text/html");
        httpd_resp_send(r, response_html, strlen(response_html));
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Unknown mode: %s", mode_str);
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Unknown mode");
        return ESP_FAIL;
    }
}


// Definition of device_mode_switch_handler (for buttons on /fs page)
esp_err_t device_mode_switch_handler(httpd_req_t *r) {
    if (current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) {
        return server_busy_response(r, "Device is in AP Configuration mode. Cannot switch SD card access.");
    }
    if (usb_fs_busy) {
        return server_busy_response(r, "USB file system is busy. Cannot switch SD card access now.");
    }

    char content_type[100];
    if (httpd_req_get_hdr_value_str(r, "Content-Type", content_type, sizeof(content_type)) != ESP_OK ||
        strstr(content_type, "application/x-www-form-urlencoded") == NULL) {
        ESP_LOGE(TAG, "Invalid Content-Type for mode switch");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid Content-Type");
        return ESP_FAIL;
    }

    char buf[64];
    int ret = httpd_req_recv(r, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(r);
        } else {
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request data");
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char mode_str[16];
    if (httpd_query_key_value(buf, "mode", mode_str, sizeof(mode_str)) != ESP_OK) {
        ESP_LOGE(TAG, "Mode parameter not found");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Mode parameter missing");
        return ESP_FAIL;
    }

    if (strcmp(mode_str, "wifi") == 0) {
        if (op_mode == HTTP_MODE) {
            ESP_LOGI(TAG, "Already in WiFi (HTTP) mode.");
            httpd_resp_sendstr(r, "Already in WiFi (HTTP) mode.");
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Switching to WiFi (HTTP) mode...");
        op_mode = HTTP_MODE; // Update global state
        trigger_mode_switch_req_cb(NULL); // Kích hoạt chuyển đổi GPIO cho ESP32
    } else if (strcmp(mode_str, "usb") == 0) {
        if (op_mode == USB_MODE) {
            ESP_LOGI(TAG, "Already in USB (CNC/PC) mode.");
            httpd_resp_sendstr(r, "Already in USB (CNC/PC) mode.");
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Switching to USB (CNC/PC) mode...");
        op_mode = USB_MODE; // Update global state
        trigger_mode_switch_req_cb(NULL); // Kích hoạt chuyển đổi GPIO cho USB MSC
    } else {
        ESP_LOGE(TAG, "Unknown mode: %s", mode_str);
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Unknown mode");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(r, "Mode switched successfully.");
    return ESP_OK;
}

esp_err_t esp_resp_dir_html(httpd_req_t *r, const char* file_path){

    DIR *dir = opendir(file_path);
    struct dirent *entry;

    if(!dir){
        ESP_LOGE(TAG, "Read path, invalid: %s", file_path);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid path");
        return ESP_FAIL;
    }

    httpd_resp_sendstr_chunk(r, head1);
    httpd_resp_sendstr_chunk(r, hostname);
    httpd_resp_sendstr_chunk(r, head2);
    
    // Determine button styles based on current op_mode
    const char *wifi_button_style = (op_mode == HTTP_MODE) ? "bg-green-600 text-white" : "border-green-600 text-green-600 hover:bg-green-100";
    const char *usb_button_style = (op_mode == USB_MODE) ? "bg-blue-600 text-white" : "border-blue-600 text-blue-600 hover:bg-blue-100";
    
    char full_upload_html[1500]; // Make sure this buffer is large enough
    snprintf(full_upload_html, sizeof(full_upload_html), upload_and_mode_control, wifi_button_style, usb_button_style);
    httpd_resp_sendstr_chunk(r, full_upload_html);


    httpd_resp_sendstr_chunk(r, "<div id=\"file-list\" class=\"p-5 space-y-2\">");

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if(entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(r, "<div class=\"file-item w-fit flex items-center space-x-2 rounded bg-yellow-100 p-2 hover:bg-yellow-200\" data-path=\"/delete/");
        } else {
            httpd_resp_sendstr_chunk(r, "<div class=\"file-item w-fit flex items-center space-x-2 rounded bg-gray-100 p-2 hover:bg-gray-200\" data-path=\"/delete/");
        }

        httpd_resp_sendstr_chunk(r, entry->d_name);
        if(entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");

        httpd_resp_sendstr_chunk(r, "\" data-href=\"");
        // Ensure links are relative to /fs, not directly from root
        httpd_resp_sendstr_chunk(r, "/fs"); // Added /fs prefix
        httpd_resp_sendstr_chunk(r, r->uri + strlen("/fs")); // Adjust URI to remove /fs for internal use
        httpd_resp_sendstr_chunk(r, entry->d_name);
        if(entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");
        httpd_resp_sendstr_chunk(r, "\" oncontextmenu=\"showContextMenu(event, this)\">");

        if (strcmp(entry->d_name, ".config") != 0) {
            httpd_resp_sendstr_chunk(r, "<input type=\"checkbox\" class=\"file-checkbox\" data-path=\"/delete/");
            httpd_resp_sendstr_chunk(r, entry->d_name);
            if (entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");
            httpd_resp_sendstr_chunk(r, "\">");
        }

        if(entry->d_type == DT_DIR){
            httpd_resp_sendstr_chunk(r,
                "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"h-5 w-5 text-yellow-600\" fill=\"none\" viewBox=\"0 0 24 24\" stroke=\"currentColor\">"
                "<path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M3 7h5l2 2h11v9H3V7z\" />"
                "</svg>"
            );
        } else {
            httpd_resp_sendstr_chunk(r,
                "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"h-5 w-5\" fill=\"none\" viewBox=\"0 0 24 24\" stroke=\"currentColor\">"
                "<path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M7 0H2V16H14V7H7V0Z\" />"
                "</svg>"
            );
        }

        char file_link[MAX_URI_AND_NAME_LEN];

        httpd_resp_sendstr_chunk(r, "<a href=\"");
        
        // Adjust links for /edit and other file operations
        if (entry->d_type != DT_DIR && is_editable(entry->d_name)) {
            // If editable file, point to /edit/fs/...
            snprintf(file_link, sizeof(file_link), "/edit/fs%s%s", r->uri + strlen("/fs"), entry->d_name);
        } else {
            // If directory or non-editable file, point to /fs/...
            snprintf(file_link, sizeof(file_link), "/fs%s%s", r->uri + strlen("/fs"), entry->d_name);
            if (entry->d_type == DT_DIR) {
                strlcat(file_link, "/", sizeof(file_link));
            }
        }
        
        httpd_resp_sendstr_chunk(r, file_link);
        httpd_resp_sendstr_chunk(r, "\" class=\"text-gray-800 hover:underline\">");
        httpd_resp_sendstr_chunk(r, entry->d_name);
        httpd_resp_sendstr_chunk(r, "</a>");

        httpd_resp_sendstr_chunk(r, "</div>");
    }

    httpd_resp_sendstr_chunk(r, "</div>");

    closedir(dir);
    httpd_resp_sendstr_chunk(r, deletemenu);
    httpd_resp_sendstr_chunk(r, "<script src=\"/script.js\"></script>");
    // Inline script for mode switching on the /fs page itself
    httpd_resp_sendstr_chunk(r, "<script>\n"
        "function switchToMode(mode) {\n"
        "    if (confirm('Are you sure you want to switch to ' + mode.toUpperCase() + ' mode? This will change SD card access and might disconnect the current USB connection.')) {\n"
        "        fetch('/switch_device_mode', {\n"
        "            method: 'POST',\n"
        "            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },\n"
        "            body: 'mode=' + mode\n"
        "        })\n"
        "        .then(response => {\n"
        "            if (response.ok) {\n"
        "                alert('Mode switch requested. Page will reload.');\n"
        "                location.reload();\n"
        "            } else {\n"
        "                response.text().then(text => alert('Failed to switch mode: ' + text));\n"
        "            }\n"
        "        })\n"
        "        .catch(error => {\n"
        "            console.error('Error:', error);\n"
        "            alert('Error switching mode.');\n"
        "        });\n"
        "    }\n"
        "}\n"
        "function switchToWifiMode() { switchToMode('wifi'); }\n"
        "function switchToUsbMode() { switchToMode('usb'); }\n"
        "</script>");

    httpd_resp_sendstr_chunk(r, "</body></html>");
    httpd_resp_send_chunk(r, NULL, 0);

    ESP_LOGI(TAG, "Directory listing for %s sent.", file_path);
    return ESP_OK;
}

static httpd_handle_t file_server_handle = NULL;

esp_err_t download_handler(httpd_req_t *r){
    if(current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) return server_busy_response(r, "Device is in AP Configuration mode. Please go to 192.168.4.1 to configure WiFi.");
    if(op_mode == USB_MODE) return server_busy_response(r, "Device is on USB mode. HTTP functions are disabled.");
    if(usb_fs_busy) return server_busy_response(r, "USB is currently used, try again later.");

    // Adjust base path for /fs
    char adjusted_uri[MAX_URI_AND_NAME_LEN];
    const char *path_in_fs = r->uri + strlen("/fs"); // Remove "/fs" prefix
    strlcpy(adjusted_uri, path_in_fs, sizeof(adjusted_uri));

    char filepath[MAX_FILE_PATH];
    FILE *file = NULL;
    struct stat file_stat;

    // Use adjusted_uri for file path resolution
    const char* filename = get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, adjusted_uri, sizeof(filepath));
    if(!filename) {
        ESP_LOGE(TAG, "filename TOO LONG");
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "File path TOO LONG");
        return ESP_FAIL;
    }

    if(strchr(filepath, '%')){
        char fix_filepath[sizeof(filepath)];
        strncpy(fix_filepath, filepath, sizeof(filepath));
        memset(filepath, 0, sizeof(filepath));
        if(fix_space(filepath, fix_filepath)) {
            if (strlen(filepath) == 0) {
                strlcpy(filepath, fix_filepath, sizeof(filepath));
            }
        }
    }

    // Mount SD card before any file operation
    sdmmc_card_t *card = NULL;
    esp_err_t ret = mount_sdmmc(&card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD: %s", esp_err_to_name(ret));
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Mount SD failed");
        // QUAN TRỌNG: KHÔNG ĐƯỢC THAY ĐỔI OP_MODE HOẶC GPIO Ở ĐÂY.
        // Chỉ báo lỗi và cho phép người dùng tự chọn mode lại.
        return ESP_FAIL;
    }

    if(filename[strlen(filename) - 1] == '/'){
        ESP_LOGI(TAG, "File name has trailing /");
        esp_resp_dir_html(r, filepath); 
        umount_sdmmc(card);
        return ESP_OK;
    }

    if (strcmp(filename, "/favicon.ico") == 0) {
        ESP_LOGE(TAG, "ignore favicon");
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "No favicon.");
        umount_sdmmc(card);
        return ESP_FAIL;
    }


    if(stat(filepath, &file_stat) == -1){
        ESP_LOGE(TAG, "file name invalid or does not exist: %s", filepath);
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "File does not exist");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    file = fopen(filepath, "r");
    if(!file){
        ESP_LOGE(TAG, "Cannot open file: %s", filepath);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file.");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Set content type");
    set_content_type_from_file(r, filename);
  
    ESP_LOGI(TAG, "Sending file %s, size %ld bytes", filename, file_stat.st_size);
    char* chunk = ((struct file_server_buffer*)r->user_ctx)->buffer;
    size_t chunksize;

    do {
        chunksize = fread(chunk, 1, FILE_BUFSIZE, file);;
        if(chunksize > 0){
            if(httpd_resp_send_chunk(r, chunk, chunksize) != ESP_OK){
                fclose(file);
                ESP_LOGE(TAG, "Fail to send file, abort!");
                httpd_resp_sendstr_chunk(r, NULL);
                httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Fail while sending file.");
                umount_sdmmc(card);
                return ESP_FAIL;
            }
        }

    } while(chunksize != 0);

    fclose(file);

    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_send_chunk(r, NULL, 0);

    ESP_LOGI(TAG, "Handle URI complete.");
    umount_sdmmc(card);
    return ESP_OK;
}

esp_err_t edit_handler(httpd_req_t *r) {
    if(current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) return server_busy_response(r, "Device is in AP Configuration mode. Please go to 192.168.4.1 to configure WiFi.");
    if(op_mode == USB_MODE) return server_busy_response(r, "Device is on USB mode. HTTP functions are disabled.");
    if(usb_fs_busy) return server_busy_response(r, "USB is currently used, try again later.");

    // Adjust base path for /edit/fs/
    char adjusted_uri[MAX_URI_AND_NAME_LEN];
    const char *path_in_fs = r->uri + strlen("/edit/fs"); // Remove "/edit/fs" prefix
    strlcpy(adjusted_uri, path_in_fs, sizeof(adjusted_uri));

    sdmmc_card_t *card = NULL;
    esp_err_t ret = mount_sdmmc(&card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD: %s", esp_err_to_name(ret));
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Mount SD failed");
        return ESP_FAIL;
    }

    char filepath[MAX_FILE_PATH];
    FILE *file = NULL;

    get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, adjusted_uri, sizeof(filepath));

    ESP_LOGI(TAG, "Editing file: %s", filepath);

    file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for editing: %s", filepath);
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "File not found");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    httpd_resp_sendstr_chunk(r, head1);
    httpd_resp_sendstr_chunk(r, "Edit File");
    httpd_resp_sendstr_chunk(r, head2);

    char form_action[MAX_FILE_PATH + 10];
    snprintf(form_action, sizeof(form_action), "/save/fs%s", r->uri + strlen("/edit/fs")); // Adjust action URL
    
    httpd_resp_sendstr_chunk(r, "<div class='p-5 space-y-4'>");
    httpd_resp_sendstr_chunk(r, "<h1 class='text-2xl font-bold'>Editing: ");
    httpd_resp_sendstr_chunk(r, r->uri + strlen("/edit/fs/")); // Adjust display path
    httpd_resp_sendstr_chunk(r, "</h1>");

    httpd_resp_sendstr_chunk(r, "<form action=\"");
    httpd_resp_sendstr_chunk(r, form_action);
    httpd_resp_sendstr_chunk(r, "\" method=\"post\">");
    
    httpd_resp_sendstr_chunk(r, "<textarea name='content' class='w-full p-2 border border-gray-300 rounded font-mono' style='height: 70vh;'>");
    
    char *chunk = ((struct file_server_buffer*)r->user_ctx)->buffer;
    size_t chunksize;
    do {
        chunksize = fread(chunk, 1, FILE_BUFSIZE - 1, file);
        if (chunksize > 0) {
            httpd_resp_send_chunk(r, chunk, chunksize);
        }
    } while (chunksize > 0);
    fclose(file);

    httpd_resp_sendstr_chunk(r, "</textarea>");

    httpd_resp_sendstr_chunk(r, "<div class='mt-4 flex space-x-2'>");
    httpd_resp_sendstr_chunk(r, "<button type='submit' class='rounded border border-sky-600 text-sky-600 px-4 py-2 hover:bg-sky-100'>Save Changes</button>");
    httpd_resp_sendstr_chunk(r, "<a href='/fs' class='rounded bg-gray-200 px-4 py-2 text-black shadow hover:bg-gray-300'>Cancel</a>"); // Adjust cancel link
    httpd_resp_sendstr_chunk(r, "</div>");

    httpd_resp_sendstr_chunk(r, "</form></div></body></html>");

    httpd_resp_send_chunk(r, NULL, 0);

    umount_sdmmc(card);
    return ESP_OK;
}

esp_err_t save_handler(httpd_req_t *r) {
    if(current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) return server_busy_response(r, "Device is in AP Configuration mode. Please go to 192.168.4.1 to configure WiFi.");
    if(op_mode == USB_MODE) return server_busy_response(r, "Device is on USB mode. HTTP functions are disabled.");
    if(usb_fs_busy) return server_busy_response(r, "USB is currently used, try again later.");

    // Adjust base path for /save/fs/
    char adjusted_uri[MAX_URI_AND_NAME_LEN];
    const char *path_in_fs = r->uri + strlen("/save/fs"); // Remove "/save/fs" prefix
    strlcpy(adjusted_uri, path_in_fs, sizeof(adjusted_uri));

    sdmmc_card_t *card = NULL;
    esp_err_t ret = mount_sdmmc(&card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD: %s", esp_err_to_name(ret));
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Mount SD failed");
        return ESP_FAIL;
    }

    char filepath[MAX_FILE_PATH];
    FILE *file = NULL;

    get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, adjusted_uri, sizeof(filepath));

    char *content_buf = malloc(r->content_len + 1);
    if (!content_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for save content");
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    ret = httpd_req_recv(r, content_buf, r->content_len);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(r);
        }
        free(content_buf);
        umount_sdmmc(card);
        return ESP_FAIL;
    }
    content_buf[ret] = '\0';

    char *content_start = strstr(content_buf, "content=");
    if (!content_start) {
        ESP_LOGE(TAG, "Invalid POST data format");
        free(content_buf);
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid form data");
        umount_sdmmc(card);
        return ESP_FAIL;
    }
    content_start += strlen("content=");

    char *decoded_content = malloc(strlen(content_start) + 1);
    if (!decoded_content) {
        ESP_LOGE(TAG, "Failed to allocate memory for decoded content");
        free(content_buf);
        umount_sdmmc(card);
        return ESP_FAIL;
    }
    
    if (url_decode(content_start, decoded_content, strlen(content_start) + 1) != ESP_OK) {
        ESP_LOGE(TAG, "URL decoding failed");
        free(content_buf);
        free(decoded_content);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to decode content");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    file = fopen(filepath, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath);
        free(content_buf);
        free(decoded_content);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not open file for writing");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    fwrite(decoded_content, 1, strlen(decoded_content), file);
    fclose(file);
    ESP_LOGI(TAG, "File saved successfully: %s", filepath);

    free(content_buf);
    free(decoded_content);
    
    if(restart_usb_after_write && remount_task_handler != NULL) {
        xTaskNotify(remount_task_handler, 1, eSetBits);
    }

    httpd_resp_set_status(r, "303 See Other");
    httpd_resp_set_hdr(r, "Location", "/fs"); // Adjust redirect location
    httpd_resp_send(r, NULL, 0);

    umount_sdmmc(card);
    return ESP_OK;
}

esp_err_t file_upload_handler(httpd_req_t *r){
    if(current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) return server_busy_response(r, "Device is in AP Configuration mode. Please go to 192.168.4.1 to configure WiFi.");
    if(op_mode == USB_MODE) return server_busy_response(r, "Device is on USB mode. HTTP functions are disabled.");
    if(usb_fs_busy) return server_busy_response(r, "USB is currently used, try again later.");

    // Adjust base path for /upload/fs/
    char adjusted_uri[MAX_URI_AND_NAME_LEN];
    const char *path_in_fs = r->uri + strlen("/upload/fs"); // Remove "/upload/fs" prefix
    strlcpy(adjusted_uri, path_in_fs, sizeof(adjusted_uri));

    sdmmc_card_t *card = NULL;
    esp_err_t ret = mount_sdmmc(&card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD: %s", esp_err_to_name(ret));
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Mount SD failed");
        return ESP_FAIL;
    }

    char filepath[MAX_FILE_PATH];
    struct stat file_stat;
    FILE *file = NULL;

    const char* filename = get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, adjusted_uri, sizeof(filepath));

    if(!filename){
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "File name too long!");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    if(strchr(filepath, '%')){
        char fix_filepath[sizeof(filepath)];
        strncpy(fix_filepath, filepath, sizeof(filepath));
        memset(filepath, 0, sizeof(filepath));
        if(fix_space(filepath, fix_filepath))
            strncpy(filepath, fix_filepath, sizeof(filepath));
    }

    if(filename[strlen(filename) - 1] == '/'){
        ESP_LOGE(TAG, "File name has trailing /.");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "File name has /");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    if(stat(filepath, &file_stat) == 0){
        ESP_LOGE(TAG, "File already exsist!");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "File already exsist!");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    file = fopen(filepath, "w");
    if(!file){
        ESP_LOGE(TAG, "Cannot create file. %s", filepath);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file.");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Recieving file...");

    char* buffer = ((struct file_server_buffer*)r->user_ctx)->buffer;

    size_t remaining = r->content_len;
    size_t recieved = 0;
    while(remaining > 0){
        http_busy = 1;

        if(usb_seize_request){
            fclose(file);
            unlink(filepath);

            usb_seize_request = 0;
            http_busy = 0;

            ESP_LOGE(TAG, "File reception failed.");
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "USB interrupt");
            ESP_LOGE(TAG, "Force close.");
            umount_sdmmc(card);
            return ESP_OK;
        }

        if((recieved = httpd_req_recv(r, buffer, MIN(remaining, FILE_BUFSIZE))) <= 0 ){
            if(recieved == HTTPD_SOCK_ERR_TIMEOUT){
                ESP_LOGE(TAG, "Timeout, retry");
                continue;
            }

            fclose(file);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed.");
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "File reception failed.");
            http_busy = 0;
            umount_sdmmc(card);
            return ESP_OK;
        }

        if(recieved > FILE_BUFSIZE){
            ESP_LOGE(TAG, "chunk too large");
            continue;
        }

        if(recieved && (recieved != fwrite(buffer, 1, recieved, file))){
            fclose(file);
            unlink(filepath);

            ESP_LOGE(TAG, "Fail to write file to storage.");
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Fail to write file to storage.");
            http_busy = 0;
            umount_sdmmc(card);
            return ESP_OK;
        }

        remaining -= recieved;
    }

    fclose(file);
    http_busy = 0;
    ESP_LOGI(TAG, "File transfer completed.");

    httpd_resp_set_status(r, "303 See Other");
    httpd_resp_set_hdr(r, "Location", "/fs"); // Adjust redirect location
    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_sendstr(r, NULL);

    if(restart_usb_after_write && remount_task_handler != NULL) xTaskNotify(remount_task_handler, 1, eSetBits);

    umount_sdmmc(card);
    return ESP_OK;
}

esp_err_t file_delete_handler(httpd_req_t *r){
    if(current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) return server_busy_response(r, "Device is in AP Configuration mode. Please go to 192.168.4.1 to configure WiFi.");
    if(op_mode == USB_MODE) return server_busy_response(r, "Device is on USB mode. HTTP functions are disabled.");
    if(usb_fs_busy) return server_busy_response(r, "USB is currently used, try again later.");

    // Adjust base path for /delete/fs/
    char adjusted_uri[MAX_URI_AND_NAME_LEN];
    const char *path_in_fs = r->uri + strlen("/delete/fs"); // Remove "/delete/fs" prefix
    strlcpy(adjusted_uri, path_in_fs, sizeof(adjusted_uri));

    sdmmc_card_t *card = NULL;
    esp_err_t ret = mount_sdmmc(&card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD: %s", esp_err_to_name(ret));
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Mount SD failed");
        return ESP_FAIL;
    }

    
    char filepath[MAX_FILE_PATH];
    struct stat file_stat;

    const char* filename = get_path_from_uri(filepath, ((struct file_server_buffer *)r->user_ctx)->base_path, adjusted_uri, sizeof(filepath));
    ESP_LOGI(TAG, "Filename %s\n", filename);

    if(strchr(filepath, '%')){
        char fix_filepath[sizeof(filepath)];
        strncpy(fix_filepath, filepath, sizeof(filepath));
        memset(filepath, 0, sizeof(filepath));
        if(fix_space(filepath, fix_filepath)) strncpy(filepath, fix_filepath, sizeof(filepath));
    }

    if(!filename){
        ESP_LOGE(TAG, "Filename too long!");
        httpd_resp_send_err(r, HTTPD_414_URI_TOO_LONG, "Filename too long.");
        umount_sdmmc(card);
        return ESP_FAIL;
    }


    if(stat(filepath, &file_stat) == -1){
        ESP_LOGE(TAG, "File doesn't exsist.");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "File doesn't exsist.");
        umount_sdmmc(card);
        return ESP_FAIL;
    }

    if(filename[strlen(filename) - 1] == '/'){ 
        filepath[strlen(filepath)-1] = '\0';
        printf("%s\n\r", filepath);
        ESP_LOGE(TAG, "Dir delete %d.", rmdir(filepath));
        httpd_resp_set_status(r, "303 See Other");
        httpd_resp_set_hdr(r, "Location", "/fs"); // Adjust redirect location
        httpd_resp_sendstr(r, NULL);
        umount_sdmmc(card);
        return ESP_OK;
    }
    unlink(filepath);

    ESP_LOGI(TAG, "File deleted.");
    httpd_resp_set_status(r, "303 See Other");
    httpd_resp_set_hdr(r, "Location", "/fs"); // Adjust redirect location
    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_sendstr(r, NULL);

    if(restart_usb_after_write && remount_task_handler != NULL) xTaskNotify(remount_task_handler, 1, eSetBits);

    umount_sdmmc(card);
    return ESP_OK;
}

esp_err_t remount_handler(httpd_req_t *r) {
    if(current_wifi_op_mode == WIFI_MODE_AP_ACTIVE) return server_busy_response(r, "Device is in AP Configuration mode. Please go to 192.168.4.1 to configure WiFi.");
    if (remount_task_handler != NULL) {
        ESP_LOGI(TAG, "Remount requested via HTTP.");
        xTaskNotify(remount_task_handler, 1, eSetBits);
    } else {
        ESP_LOGW(TAG, "Remount task not running.");
    }
    httpd_resp_sendstr(r, "Remount triggered.");
    return ESP_OK;
}

extern int32_t delay;
extern char ip_str[16];
extern char hostname[25];
esp_err_t ping_get_handler(httpd_req_t *req) {
    char str[100] = {0};
    sprintf(str, "{\"name\":\"%s\",\"ip\":\"%s\"}", hostname, ip_str);

    return httpd_resp_sendstr(req, str);
}

extern const uint8_t output_css_start[] asm("_binary_output_css_start");
extern const uint8_t output_css_end[]   asm("_binary_output_css_end");
esp_err_t css_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    return httpd_resp_send(req,
        (const char *)output_css_start,
        output_css_end - output_css_start);
}

extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[]   asm("_binary_script_js_end");
esp_err_t script_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/javascript"); // Correct MIME type for JS
    return httpd_resp_send(req,
        (const char *)script_js_start,
        script_js_end - script_js_start);
}

esp_err_t start_file_server(){
    static struct file_server_buffer *server_buffer = NULL;
    if(file_server_handle){ // Check against global server handle
        ESP_LOGI(TAG, "File server already started.");
        return ESP_OK; // Return OK if already running
    }
    
    // Allocate server_buffer only if it's NULL (first time start)
    if (server_buffer == NULL) {
        server_buffer = (struct file_server_buffer*)heap_caps_malloc(sizeof(struct file_server_buffer), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if(!server_buffer){
            ESP_LOGE(TAG, "Cannot allocate buffer for file server.");
            return ESP_ERR_NO_MEM;
        }
        memset(server_buffer, 0, sizeof(struct file_server_buffer));
        strlcpy(server_buffer->base_path, BASE_PATH, sizeof(server_buffer->base_path));
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = 1;
    config.stack_size = 8 * 1024;
    config.keep_alive_enable = true;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if(httpd_start(&file_server_handle, &config) != ESP_OK){ // Use file_server_handle
        ESP_LOGE(TAG, "Failed to start file server...");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Server started");

    // Register handlers for the initial mode selection page
    httpd_uri_t mode_page_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = mode_selection_page_handler,
        .user_ctx  = server_buffer
    };
    httpd_register_uri_handler(file_server_handle, &mode_page_uri);

    httpd_uri_t select_mode_post_uri = {
        .uri       = "/select_mode",
        .method    = HTTP_POST,
        .handler   = mode_selection_post_handler,
        .user_ctx  = server_buffer
    };
    httpd_register_uri_handler(file_server_handle, &select_mode_post_uri);


    // Register handlers for the actual file server interface (under /fs)
    httpd_uri_t fs_root_uri = {
        .uri       = "/fs",
        .method    = HTTP_GET,
        .handler   = download_handler, // Will handle directory listing for /fs
        .user_ctx  = server_buffer
    };
    httpd_register_uri_handler(file_server_handle, &fs_root_uri);

    httpd_uri_t fs_wildcard_uri = {
        .uri       = "/fs/*", // Wildcard for files/folders under /fs
        .method    = HTTP_GET,
        .handler   = download_handler,
        .user_ctx  = server_buffer
    };
    httpd_register_uri_handler(file_server_handle, &fs_wildcard_uri);

    httpd_uri_t css_uri = { .uri = "/output.css", .method = HTTP_GET, .handler = css_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(file_server_handle, &css_uri);

    httpd_uri_t ping_uri = { .uri = "/id", .method = HTTP_GET, .handler = ping_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(file_server_handle, &ping_uri);

    httpd_uri_t js_uri = { .uri = "/script.js", .method = HTTP_GET, .handler = script_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(file_server_handle, &js_uri);
    
    // File operations now under /fs or specifically handled
    httpd_uri_t file_upload = { .uri = "/upload/fs/*", .handler = file_upload_handler, .method = HTTP_POST, .user_ctx = server_buffer };
    httpd_register_uri_handler(file_server_handle, &file_upload);
    
    httpd_uri_t file_delete = { .uri = "/delete/fs/*", .handler = file_delete_handler, .method = HTTP_POST, .user_ctx = server_buffer };
    httpd_register_uri_handler(file_server_handle, &file_delete);

    httpd_uri_t file_edit = { .uri = "/edit/fs/*", .handler = edit_handler, .method = HTTP_GET, .user_ctx = server_buffer };
    httpd_register_uri_handler(file_server_handle, &file_edit);

    httpd_uri_t file_save = { .uri = "/save/fs/*", .handler = save_handler, .method = HTTP_POST, .user_ctx = server_buffer };
    httpd_register_uri_handler(file_server_handle, &file_save);

    httpd_uri_t remount_uri = { .uri = "/remount", .method = HTTP_POST, .handler = remount_handler, .user_ctx = NULL };
    httpd_register_uri_handler(file_server_handle, &remount_uri);

    httpd_uri_t device_mode_switch_uri = {
        .uri = "/switch_device_mode",
        .method = HTTP_POST,
        .handler = device_mode_switch_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(file_server_handle, &device_mode_switch_uri);

    // The generic /* handler will not be used if specific paths like /fs/* are matched first.
    // If you need it, ensure its handler can correctly distinguish between file operations and other requests.
    // For now, rely on specific handlers under /fs.

    return ESP_OK;
}