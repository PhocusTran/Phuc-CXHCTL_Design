#include "esp_compiler.h"
#include "esp_err.h"
#include "esp_log.h"
#include "file_server.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Cần cho strerror và strcat
#include <unistd.h>
#include "esp_http_server.h"
#include "freertos/idf_additions.h"
#include "http_parser.h"
#include "lock.h"
#include "main.h"
#include "task.h"
#include "sys/dirent.h"
#include "mount.h"

#include "tusb_msc_storage.h"
#include "extran_event_callback.h"
#include "usb_msc.h"
#include "esp_http_client.h" // Cần cho url_decode
// Lấy giá trị từ Kconfig, mặc định là 512
#include "sdkconfig.h" 
// Tên file tối đa, thường là 255 trong vfs
#include "sys/param.h" 
#include "esp_system.h" 
#include <errno.h> // Cần cho errno
#include "esp_heap_caps.h" // Để kiểm tra free heap
#include <inttypes.h> // Cần cho PRIu32 (cho các macro ESP_LOG)

#define MAX_URI_AND_NAME_LEN (CONFIG_HTTPD_MAX_URI_LEN + NAME_MAX + 10)
#define FILE_PATH_MAX 256

static const char* TAG = "FILE";
extern uint8_t op_mode;

extern uint8_t restart_usb_after_write;

extern TaskHandle_t remount_task_handler;
extern char hostname[25];

extra_event_callback mode_switch_req_cb = {0};

/* --- START HELPER FUNCTIONS --- */
// Hàm kiểm tra xem file có phải là file text có thể sửa được không
static bool is_editable(const char *filename) {
    const char *editable_extensions[] = {
        ".txt", ".c", ".nc", ".h", ".html", ".js", ".css", ".json", ".md", ".log", ".config", ".py", ".cpp",
        ".err", ".anc", ".tap", ".gcode", ".cnc", ".iso", ".ngc", ".eia", ".cam", ".apt", ".cl", ".cln", ".nci", ".set", ".xilog", ".sbp"
    };
    for (int i = 0; i < sizeof(editable_extensions) / sizeof(editable_extensions[0]); i++) {
        // Tìm lần xuất hiện cuối cùng của '.' để so sánh phần đuôi file
        const char *dot = strrchr(filename, '.');
        if (dot && strcasecmp(dot, editable_extensions[i]) == 0) {
            return true;
        }
    }
    // Cho phép sửa file không có đuôi, ví dụ file ".config" của bạn
    if (strcmp(filename, ".config") == 0) return true;
    
    return false;
}

// Hàm để URL-decode chuỗi nhận từ form HTML
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
                return ESP_FAIL; // Invalid encoding
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
/* --- END HELPER FUNCTIONS --- */


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

// Đã loại bỏ form delete khỏi context menu HTML
const char *deletemenu = "<div id=\"context-menu\" class=\"hidden fixed z-9999 bg-white border rounded shadow-md w-32 text-sm\"></div>\r\n";

const char *head1 = 
    "<!doctype html>\n<html lang=\"en\">\n    <head>\n        <meta charset=\"UTF-8\" />\n        <title>";

const char *head2 = "</title>\n        <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\" />\n        <meta name=\"description\" content=\"\" />\n        <link rel=\"icon\" href=\"favicon.png\" />\n <link href=\"/output.css\" rel=\"stylesheet\"> \n    </head>\n <body>";

const char *upload =
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
"\n"
"        </div>\n";


const char* change_mode_btn = "<form method=\"POST\" style=\"margin:2em;\" action=\"/mode\"><button type=\"submit\" name=\"toggle\" style=\"padding:1em;font-size:120%;\">";

esp_err_t index_redirect_handler(httpd_req_t *r){
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "/");
    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_send(r, NULL, 0);
    return ESP_OK;
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
    if(!p || (sizeof(dest) < sizeof(src)) || !(p[1]=='2') || !(p[2]=='0')) return 1;

    strncpy(dest, src, p - src);
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



// check file extension
#define EXT_CHECK(filename, ext) \
    strcasecmp(&filename[strlen(filename) - strlen(ext)], ext)

esp_err_t set_content_type_from_file(httpd_req_t *r, const char* filename){
    if(EXT_CHECK(filename, ".pdf") == 0) {
        return httpd_resp_set_type(r, "application/pdf");
    } else if(EXT_CHECK(filename, ".html") == 0){
        return httpd_resp_set_type(r, "text/html");
    } else if(EXT_CHECK(filename, ".jpeg") == 0){
        return httpd_resp_set_type(r, "image/jpeg");
    } else if(EXT_CHECK(filename, ".jpg") == 0){
        return httpd_resp_set_type(r, "image/jpg");
    } else if(EXT_CHECK(filename, ".png") == 0){
        return httpd_resp_set_type(r, "image/png");
    }
    return httpd_resp_set_type(r, "text/plain");
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
    httpd_resp_sendstr_chunk(r, upload);
    httpd_resp_sendstr_chunk(r, "<div id=\"file-list\" class=\"p-5 space-y-2\">");

    while ((entry = readdir(dir)) != NULL) {

        // Bỏ qua các entry "." và ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Bắt đầu div file-item
        if(entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(r, "<div class=\"file-item w-fit flex items-center space-x-2 rounded bg-yellow-100 p-2 hover:bg-yellow-200\" data-path=\"/delete/");
        } else {
            httpd_resp_sendstr_chunk(r, "<div class=\"file-item w-fit flex items-center space-x-2 rounded bg-gray-100 p-2 hover:bg-gray-200\" data-path=\"/delete/");
        }

        // Đường dẫn delete
        httpd_resp_sendstr_chunk(r, entry->d_name);
        if(entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");

        httpd_resp_sendstr_chunk(r, "\" data-href=\"");
        httpd_resp_sendstr_chunk(r, r->uri);
        httpd_resp_sendstr_chunk(r, entry->d_name);
        if(entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");
        httpd_resp_sendstr_chunk(r, "\" oncontextmenu=\"showContextMenu(event, this)\">"); 

        // Thêm checkbox HOẶC ẩn hoàn toàn nếu là file .config
        if (strcmp(entry->d_name, ".config") != 0) {
            httpd_resp_sendstr_chunk(r, "<input type=\"checkbox\" name=\"selected\" value=\"");
            httpd_resp_sendstr_chunk(r, entry->d_name);
            if (entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");
            httpd_resp_sendstr_chunk(r, "\" class=\"file-checkbox\">");
        } else {
            // Ẩn hoàn toàn checkbox cho .config bằng cách không render nó hoặc dùng một div ẩn
            // Cách đơn giản nhất là không in ra gì cả hoặc in ra một span có class để ẩn bằng CSS
            httpd_resp_sendstr_chunk(r, "<span class=\"hidden-file-checkbox\"></span>");
        }

        // SVG icon
        // if(entry->d_type == DT_DIR){
        //     httpd_resp_sendstr_chunk(r, "<span class=\"text-xl\">📁</span>"); 
        // } else {
        //     httpd_resp_sendstr_chunk(r, "<span class=\"text-xl\">📄</span>");  
        // }

        // Tên file - TẠO LINK ĐỘNG
        // Khai báo buffer đủ lớn để chứa URI + tên file + tiền tố
        char file_link[MAX_URI_AND_NAME_LEN];

        // Mở thẻ <a>
        httpd_resp_sendstr_chunk(r, "<a href=\"");

        if (entry->d_type != DT_DIR && is_editable(entry->d_name)) {
            // Nếu là file CÓ THỂ SỬA, trỏ tới /edit/...
            snprintf(file_link, sizeof(file_link), "/edit%s%s", r->uri, entry->d_name);
        } else {
            // Nếu là THƯ MỤC hoặc file KHÔNG THỂ SỬA, trỏ tới link gốc để xem/tải
            snprintf(file_link, sizeof(file_link), "%s%s", r->uri, entry->d_name);
            if (entry->d_type == DT_DIR) {
                // Thêm dấu / cho thư mục
                strlcat(file_link, "/", sizeof(file_link));
            }
        }

        // Gửi link đã được tạo, class và tên file
        httpd_resp_sendstr_chunk(r, file_link);
        httpd_resp_sendstr_chunk(r, "\" class=\"text-gray-800 hover:underline\">");
        httpd_resp_sendstr_chunk(r, entry->d_name);
        httpd_resp_sendstr_chunk(r, "</a>");

        // Kết thúc div file-item
        httpd_resp_sendstr_chunk(r, "</div>");
    }

    httpd_resp_sendstr_chunk(r, "</div>");

    closedir(dir);
    // Vẫn render deletemenu, nhưng nó đã rỗng
    httpd_resp_sendstr_chunk(r, deletemenu); 
    httpd_resp_sendstr_chunk(r, "<script src=\"/script.js\"></script>");

    httpd_resp_sendstr_chunk(r, "</body></html>");
    httpd_resp_send_chunk(r, NULL, 0);

    ESP_LOGI(TAG, "Directory listing for %s sent.", file_path); 
    return ESP_OK;
}


esp_err_t usb_mode_block_response(httpd_req_t *r){
    httpd_resp_sendstr_chunk(r, "<!DOCTYPE html><html><body>");
    httpd_resp_sendstr_chunk(r, "<h2>Device is on USB mode. HTTP functions are disabled.</h2>  ");
    httpd_resp_sendstr_chunk(r, change_mode_btn);
    httpd_resp_sendstr_chunk(r, !op_mode? "Change to USB mode</button>" : "Change to HTTP mode</button>\n");
    httpd_resp_sendstr_chunk(r, "</body></html>");
    httpd_resp_send_chunk(r, NULL, 0);

    return ESP_OK;
}


esp_err_t download_handler(httpd_req_t *r){
    if(op_mode == USB_MODE) return usb_mode_block_response(r);
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED,"USB is currently used, tried again later");

    esp_err_t ret = ESP_OK;
    char filepath[MAX_FILE_PATH];
    FILE *file = NULL;
    struct stat file_stat;

    const char* filename = get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, r->uri,sizeof(filepath));
    if(!filename) {
        ESP_LOGE(TAG, "filename TOO LONG");
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "File path TOO LONG");
        return ESP_FAIL;
    }

    if(strchr(filepath, '%')){
        char fix_filepath[sizeof(filepath)];
        strncpy(fix_filepath, filepath, sizeof(filepath));
        memset(filepath, 0, sizeof(filepath));
        if(fix_space(filepath, fix_filepath)) strncpy(filepath, fix_filepath, sizeof(filepath));
    }

    if(filename[strlen(filename) - 1] == '/'){
        ESP_LOGI(TAG, "File name has trailing /");
        return esp_resp_dir_html(r, filepath);
    }

    if (strcmp(filename, "/favicon.ico") == 0) {
        ESP_LOGE(TAG, "ignore favicon");
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "No favicon.");
        return ESP_FAIL;
    }


    if(stat(filepath, &file_stat) == -1){
        ESP_LOGE(TAG, "file name invalid");
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    file = fopen(filepath, "r");
    if(!file){
        ESP_LOGE(TAG, "Cannot open file.");
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file.");
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
                return ESP_FAIL;
            }
        }

    } while(chunksize != 0);

    fclose(file);

    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_send_chunk(r, NULL, 0);

    ESP_LOGI(TAG, "Handle URI complete.");

    return ret;
}

/* --- START EDIT/SAVE HANDLERS --- */
esp_err_t edit_handler(httpd_req_t *r) {
    if(op_mode == USB_MODE) return usb_mode_block_response(r);
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED, "USB is currently used, try again later");

    char filepath[MAX_FILE_PATH];
    FILE *file = NULL;

    // Lấy đường dẫn file từ URI (bỏ qua "/edit")
    get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, r->uri + strlen("/edit"), sizeof(filepath));

    ESP_LOGI(TAG, "Editing file: %s", filepath);

    file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for editing: %s", filepath);
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    // Bắt đầu gửi trang HTML
    httpd_resp_sendstr_chunk(r, head1);
    httpd_resp_sendstr_chunk(r, "Edit File");
    httpd_resp_sendstr_chunk(r, head2);

    // Tạo form để lưu lại file
    char form_action[MAX_FILE_PATH + 10];
    snprintf(form_action, sizeof(form_action), "/save%s", r->uri + strlen("/edit"));
    
    httpd_resp_sendstr_chunk(r, "<div class='p-5 space-y-4'>");
    httpd_resp_sendstr_chunk(r, "<h1 class='text-2xl font-bold'>Editing: ");
    httpd_resp_sendstr_chunk(r, r->uri + strlen("/edit/"));
    httpd_resp_sendstr_chunk(r, "</h1>");

    httpd_resp_sendstr_chunk(r, "<form action=\"");
    httpd_resp_sendstr_chunk(r, form_action);
    httpd_resp_sendstr_chunk(r, "\" method=\"post\">");
    
    // Textarea để chứa nội dung file
    httpd_resp_sendstr_chunk(r, "<textarea name='content' class='w-full p-2 border border-gray-300 rounded font-mono' style='height: 70vh;'>");
    
    // Đọc file và gửi nội dung vào textarea
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

    // Nút Save và Cancel
    httpd_resp_sendstr_chunk(r, "<div class='mt-4 flex space-x-2'>");
    httpd_resp_sendstr_chunk(r, "<button type='submit' class='rounded border border-sky-600 text-sky-600 px-4 py-2 hover:bg-sky-100'>Save Changes</button>");
    httpd_resp_sendstr_chunk(r, "<a href='/' class='rounded bg-gray-200 px-4 py-2 text-black shadow hover:bg-gray-300'>Cancel</a>");
    httpd_resp_sendstr_chunk(r, "</div>");

    httpd_resp_sendstr_chunk(r, "</form></div></body></html>");

    // Kết thúc chunked response
    httpd_resp_send_chunk(r, NULL, 0);

    return ESP_OK;
}

esp_err_t save_handler(httpd_req_t *r) {
    if(op_mode == USB_MODE) return usb_mode_block_response(r);
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED, "USB is currently used, try again later");

    char filepath[MAX_FILE_PATH];
    FILE *file = NULL;

    get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, r->uri + strlen("/save"), sizeof(filepath));

    const char *filename_only = strrchr(filepath, '/');
    if (filename_only) filename_only++; 
    else filename_only = filepath; 

    // Debug: Log free heap trước khi cấp phát
    ESP_LOGI(TAG, "Free heap before content_buf malloc: %" PRIu32 " bytes", esp_get_free_heap_size()); // SỬA LỖI Ở ĐÂY
    ESP_LOGI(TAG, "Attempting to allocate %d bytes for content_buf", r->content_len + 1);

    char *content_buf = (char *)malloc(r->content_len + 1);
    if (!content_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for save content (%d bytes requested). Free heap: %" PRIu32, r->content_len + 1, esp_get_free_heap_size()); // SỬA LỖI Ở ĐÂY
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed for content buffer");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Allocated content_buf successfully. Free heap: %" PRIu32 " bytes", esp_get_free_heap_size()); // SỬA LỖI Ở ĐÂY


    int ret = httpd_req_recv(r, content_buf, r->content_len);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(r);
        } else {
            ESP_LOGE(TAG, "Failed to receive POST content, ret=%d", ret);
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive POST content");
        }
        free(content_buf);
        return ESP_FAIL;
    }
    content_buf[ret] = '\0';
    ESP_LOGI(TAG, "Received %d bytes of POST content.", ret);


    char *content_start = strstr(content_buf, "content=");
    if (!content_start) {
        ESP_LOGE(TAG, "Invalid POST data format: 'content=' not found.");
        free(content_buf);
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid form data");
        return ESP_FAIL;
    }
    content_start += strlen("content=");

    // Debug: Log free heap trước khi cấp phát decoded_content
    ESP_LOGI(TAG, "Free heap before decoded_content malloc: %" PRIu32 " bytes", esp_get_free_heap_size()); // SỬA LỖI Ở ĐÂY
    ESP_LOGI(TAG, "Attempting to allocate %d bytes for decoded_content (from content_start)", strlen(content_start) + 1);

    char *decoded_content = (char *)malloc(strlen(content_start) + 1);
    if (!decoded_content) {
        ESP_LOGE(TAG, "Failed to allocate memory for decoded content (%d bytes requested). Free heap: %" PRIu32, strlen(content_start) + 1, esp_get_free_heap_size()); // SỬA LỖI Ở ĐÂY
        free(content_buf);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed for decoded content");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Allocated decoded_content successfully. Free heap: %" PRIu32 " bytes", esp_get_free_heap_size()); // SỬA LỖI Ở ĐÂY
    
    if (url_decode(content_start, decoded_content, strlen(content_start) + 1) != ESP_OK) {
        ESP_LOGE(TAG, "URL decoding failed for content.");
        free(content_buf);
        free(decoded_content);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to decode content");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Content URL decoded successfully.");

    file = fopen(filepath, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s (Error: %s)", filepath, strerror(errno));
        free(content_buf);
        free(decoded_content);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not open file for writing");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "File opened for writing: %s", filepath);


    size_t written_len = fwrite(decoded_content, 1, strlen(decoded_content), file);
    if (written_len != strlen(decoded_content)) {
        ESP_LOGE(TAG, "Failed to write all content to file %s. Expected %d, wrote %zu (Error: %s)", filepath, strlen(decoded_content), written_len, strerror(errno)); // SỬA %d thành %zu cho size_t
        fclose(file);
        free(content_buf);
        free(decoded_content);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write all content to file");
        return ESP_FAIL;
    }
    fclose(file);
    ESP_LOGI(TAG, "File saved successfully: %s", filepath);

    free(content_buf);
    free(decoded_content);
    
    // Kiểm tra nếu là file ".config", thì reset ESP32
    if (strcmp(filename_only, ".config") == 0) {
        ESP_LOGI(TAG, "Config file updated, restarting ESP32...");
        // Gửi phản hồi HTTP trước khi reset để trình duyệt không bị treo
        httpd_resp_set_status(r, "200 OK"); // Hoặc 202 Accepted
        httpd_resp_sendstr(r, "Config updated, ESP32 restarting...");
        vTaskDelay(pdMS_TO_TICKS(100)); // Đợi một chút để response được gửi đi
        esp_restart(); // Reset ESP32
    } else {
        // Nếu không phải file .config, tiếp tục logic cũ: remount USB nếu cấu hình và redirect
        if(restart_usb_after_write && remount_task_handler != NULL) {
            xTaskNotify(remount_task_handler, 1, eSetBits);
        }

        httpd_resp_set_status(r, "303 See Other");
        httpd_resp_set_hdr(r, "Location", "/");
        httpd_resp_send(r, NULL, 0);
    }

    return ESP_OK;
}
/* --- END EDIT/SAVE HANDLERS --- */

esp_err_t file_upload_handler(httpd_req_t *r){
    if(usb_fs_busy) return usb_mode_block_response(r);
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED,"USB is currently used, tried again later");

    char filepath[MAX_FILE_PATH];
    struct stat file_stat;
    FILE *file = NULL;

    const char* filename = get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, r->uri + strlen("/upload"), MAX_FILE_PATH);

    if(!filename){
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "File name too long!");
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
        return ESP_FAIL;
    }

    if(stat(filepath, &file_stat) == 0){
        ESP_LOGE(TAG, "File already exsist!");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "File already exsist!");
        return ESP_FAIL;
    }

    file = fopen(filepath, "w");
    if(!file){
        ESP_LOGE(TAG, "Cannot create file. %s", filepath);
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file.");
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
            return ESP_OK;
        }

        remaining -= recieved;
    }

    fclose(file);
    http_busy = 0;
    ESP_LOGI(TAG, "File transfer completed.");

    httpd_resp_set_status(r, "303 See Other");
    httpd_resp_set_hdr(r, "Location", "/");
    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_sendstr(r, NULL);

    if(restart_usb_after_write && remount_task_handler != NULL) xTaskNotify(remount_task_handler, 1, eSetBits);

    return ESP_OK;
}

esp_err_t file_delete_handler(httpd_req_t *r){
    if(op_mode == USB_MODE) return usb_mode_block_response(r);
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED,"USB is currently used, tried again later");

    struct stat file_stat;

    // Read POST data for multiple file deletion
    char *post_data = (char *)malloc(r->content_len + 1);
    if (!post_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for POST data. Free heap: %" PRIu32, esp_get_free_heap_size()); // SỬA LỖI Ở ĐÂY
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed for POST data");
    }
    int recv_len = httpd_req_recv(r, post_data, r->content_len);
    if (recv_len <= 0) {
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(r);
        } else {
            ESP_LOGE(TAG, "Failed to receive POST data for delete, ret=%d", recv_len);
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive delete POST data");
        }
        free(post_data);
        return ESP_FAIL;
    }
    post_data[recv_len] = '\0';

    ESP_LOGI(TAG, "Received POST data for delete: %s", post_data); // Debug: Log received data

    // Find "files=" in POST data
    char *files_json_start = strstr(post_data, "files=");
    if (!files_json_start) {
        ESP_LOGE(TAG, "Missing 'files' parameter in POST data");
        free(post_data);
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Missing files parameter");
    }
    files_json_start += strlen("files=");

    // URL-decode the JSON string first
    char *decoded_json = (char *)malloc(strlen(files_json_start) + 1);
    if (!decoded_json) {
        ESP_LOGE(TAG, "Failed to allocate memory for decoded JSON. Free heap: %" PRIu32, esp_get_free_heap_size()); // SỬA LỖI Ở ĐÂY
        free(post_data);
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed for decoded JSON");
    }
    if (url_decode(files_json_start, decoded_json, strlen(files_json_start) + 1) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to URL-decode files JSON.");
        free(post_data);
        free(decoded_json);
        return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to decode JSON");
    }
    ESP_LOGI(TAG, "Decoded JSON: %s", decoded_json); // Debug: Log decoded JSON

    // Simple parsing for JSON array string: ["file1", "file2", ...]
    // This assumes the client sends a simple JSON array of strings
    // A more robust solution would use a JSON parsing library
    char *current_file_ptr = decoded_json;
    if (current_file_ptr[0] == '[') current_file_ptr++; // Skip '['
    // Remove trailing ']' if present
    if (strlen(current_file_ptr) > 0 && current_file_ptr[strlen(current_file_ptr) - 1] == ']') {
        current_file_ptr[strlen(current_file_ptr) - 1] = '\0';
    }

    char *token = strtok(current_file_ptr, ","); // Sử dụng decoded_json làm đầu vào
    while (token != NULL) {
        // Remove quotes and trim spaces from token
        char *file_to_delete_name = token;
        while (*file_to_delete_name == ' ' || *file_to_delete_name == '\"') file_to_delete_name++; // Trim leading spaces/quotes
        char *end = file_to_delete_name + strlen(file_to_delete_name) - 1;
        while (end >= file_to_delete_name && (*end == ' ' || *end == '\"')) *end-- = '\0'; // Trim trailing spaces/quotes

        // Check for .config file explicitly
        const char *filename_only = strrchr(file_to_delete_name, '/');
        if (filename_only) filename_only++; // Skip '/'
        else filename_only = file_to_delete_name; // If no '/', it's just the file name

        if (strcmp(filename_only, ".config") == 0) {
            ESP_LOGW(TAG, "Forbidden: Attempted to delete .config file: %s", file_to_delete_name);
            // Continue to next file, do not delete
            token = strtok(NULL, ",");
            continue; 
        }

        char full_path_to_delete[MAX_FILE_PATH];
        snprintf(full_path_to_delete, sizeof(full_path_to_delete), "%s/%s", ((struct file_server_buffer *)r->user_ctx)->base_path, file_to_delete_name);

        ESP_LOGI(TAG, "Attempting to delete: %s", full_path_to_delete);

        if (stat(full_path_to_delete, &file_stat) == -1) {
            ESP_LOGW(TAG, "File/directory %s does not exist, skipping.", full_path_to_delete);
        } else {
            if (file_stat.st_mode & S_IFDIR) { // Check if it's a directory
                if (rmdir(full_path_to_delete) == 0) {
                     ESP_LOGI(TAG, "Directory deleted: %s", full_path_to_delete);
                } else {
                     ESP_LOGE(TAG, "Failed to delete directory: %s (Error: %s)", full_path_to_delete, strerror(errno));
                }
            } else {
                if (unlink(full_path_to_delete) == 0) {
                    ESP_LOGI(TAG, "File deleted: %s", full_path_to_delete);
                } else {
                    ESP_LOGE(TAG, "Failed to delete file: %s (Error: %s)", full_path_to_delete, strerror(errno));
                }
            }
        }
        token = strtok(NULL, ",");
    }
    free(post_data);
    free(decoded_json); // Giải phóng bộ nhớ của chuỗi đã decode

    ESP_LOGI(TAG, "Bulk delete process completed.");
    httpd_resp_set_status(r, "303 See Other");
    httpd_resp_set_hdr(r, "Location", "/");
    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_sendstr(r, NULL);

    if(restart_usb_after_write && remount_task_handler != NULL) xTaskNotify(remount_task_handler, 1, eSetBits);
    return ESP_OK;
}

esp_err_t mode_switch_handler(httpd_req_t *r){
    if(r->content_len > 255){
        httpd_resp_send_err(r, HTTPD_414_URI_TOO_LONG, "data too long");
        ESP_LOGE(TAG, "Data len exceed limite: 100 char");
        return ESP_FAIL;
    } 

    size_t len = r->content_len;
    size_t current = 0;
    size_t recieved = 0;

    char content[255];

    while(current < len){
        recieved = httpd_req_recv(r, content + current, len - current);
        if(recieved <= 0){
            ESP_LOGE(TAG, "Error while recieving data.");
            return ESP_FAIL;
        }

        current += recieved;
    }
    content[current] = '\0';
    ESP_LOGE(TAG, "data %s", content);

    if(strncmp(content, "toggle", 3) == 0){
        trigger_mode_switch_req_cb(r);
    };


    ESP_LOGI(TAG, "operational mode %d", op_mode);
    return index_redirect_handler(r);
}

esp_err_t remount_handler(httpd_req_t *r) {
    ESP_LOGI(TAG, "remount_handler called");
    if (remount_task_handler != NULL) {
        ESP_LOGI(TAG, "Remount requested via HTTP.");
        xTaskNotify(remount_task_handler, 1, eSetBits);
    } else {
        ESP_LOGW(TAG, "Remount task not running.");
    }
    httpd_resp_sendstr(r, "Remount triggered.");
    return ESP_OK;
}

// rename_handler cần context để biết base_path
esp_err_t rename_handler(httpd_req_t *r) {
    if(op_mode == USB_MODE) return usb_mode_block_response(r);
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED, "USB is currently used, try again later");

    char old_encoded[128] = {0}, new_encoded[128] = {0};
    char oldname[128] = {0}, newname[128] = {0};
    char query[256];

    if (httpd_req_get_url_query_str(r, query, sizeof(query)) != ESP_OK) {
        ESP_LOGE(TAG, "Missing query string");
        // Trả về lỗi 400 nếu thiếu query string
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Missing query string");
    }

    ESP_LOGI(TAG, "Query string: %s", query);

    if (httpd_query_key_value(query, "old", old_encoded, sizeof(old_encoded)) != ESP_OK ||
        httpd_query_key_value(query, "new", new_encoded, sizeof(new_encoded)) != ESP_OK) {
        ESP_LOGE(TAG, "Missing 'old' or 'new' parameters");
        // Trả về lỗi 400 nếu thiếu params
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Missing old or new filename");
    }

    ESP_LOGI(TAG, "Encoded: old = %s, new = %s", old_encoded, new_encoded);

    if (url_decode(old_encoded, oldname, sizeof(oldname)) != ESP_OK ||
        url_decode(new_encoded, newname, sizeof(newname)) != ESP_OK) {
        ESP_LOGE(TAG, "URL decode failed for rename"); // Log rõ ràng hơn
        // Trả về lỗi 400 nếu decode thất bại
        return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "URL decode failed");
    }

    // Không cần loại bỏ leading slashes vì JS đã đảm bảo gửi đường dẫn tương đối
    // và chúng ta sẽ xây dựng đường dẫn đầy đủ từ base_path.

    ESP_LOGI(TAG, "Decoded: old = %s, new = %s", oldname, newname);

    char oldpath_full[FILE_PATH_MAX] = {0}, newpath_full[FILE_PATH_MAX] = {0};
    
    // Xây dựng đường dẫn đầy đủ từ base_path và tên file/folder tương đối
    // (struct file_server_buffer*)r->user_ctx cần phải được truyền vào khi đăng ký handler cho rename_uri
    snprintf(oldpath_full, sizeof(oldpath_full), "%s/%s", ((struct file_server_buffer*)r->user_ctx)->base_path, oldname);
    snprintf(newpath_full, sizeof(newpath_full), "%s/%s", ((struct file_server_buffer*)r->user_ctx)->base_path, newname);

    ESP_LOGI(TAG, "Full paths: %s -> %s", oldpath_full, newpath_full);

    if (rename(oldpath_full, newpath_full) != 0) {
        // Log lỗi chi tiết hơn từ errno
        ESP_LOGE(TAG, "Rename failed: %s -> %s (Error: %s)", oldpath_full, newpath_full, strerror(errno));
        // Gửi phản hồi lỗi 500 với thông báo chi tiết
        char err_msg[100]; // Buffer để chứa thông báo lỗi. Kích thước đủ lớn cho "Rename failed: " + strerror max length
        snprintf(err_msg, sizeof(err_msg), "Rename failed: %s", strerror(errno));
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, err_msg); // SỬA LỖI Ở ĐÂY
        return ESP_FAIL; // Quan trọng: Trả về ESP_FAIL để báo hiệu thất bại
    }

    ESP_LOGI(TAG, "Renamed successfully: %s -> %s", oldpath_full, newpath_full);
    
    // Gửi phản hồi thành công
    httpd_resp_set_status(r, "200 OK"); // Trạng thái HTTP 200 OK
    httpd_resp_sendstr(r, "Rename successful"); // Thông báo thành công cụ thể để JS kiểm tra

    // Kích hoạt remount nếu cấu hình được bật
    // Việc này sẽ được xử lý ở phía JavaScript sau khi nhận phản hồi thành công
    // if(restart_usb_after_write && remount_task_handler != NULL) {
    //     ESP_LOGI(TAG, "Triggering USB remount after rename.");
    //     xTaskNotify(remount_task_handler, 1, eSetBits);
    // }
    
    return ESP_OK; // Quan trọng: Trả về ESP_OK khi thành công
}

extern int delay;
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
    if(server_buffer){
        ESP_LOGE(TAG, "Server already started");
        return ESP_ERR_INVALID_STATE;
    }
    server_buffer = (struct file_server_buffer*)heap_caps_malloc(sizeof(struct file_server_buffer), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if(!server_buffer){
        ESP_LOGE(TAG, "Cannot allocate buffer for file server.");
        return ESP_ERR_NO_MEM;
    }
    memset(server_buffer, 0, sizeof(struct file_server_buffer));
    strlcpy(server_buffer->base_path, BASE_PATH, sizeof(server_buffer->base_path));
    
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = 1;
    config.stack_size = 8 * 1024;
    config.keep_alive_enable = true;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if(httpd_start(&server, &config) != ESP_OK){
        ESP_LOGE(TAG, "Failed to start file server...");
        free(server_buffer); // Giải phóng bộ nhớ nếu server không start được
        server_buffer = NULL;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Server started");

    // Đăng ký handler cho đường dẫn gốc "/"
    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = download_handler, // Dùng lại download_handler vì nó đã có logic xử lý thư mục
        .user_ctx  = server_buffer
    };
    httpd_register_uri_handler(server, &root_uri);

    // Đăng ký các handler cụ thể khác
    httpd_uri_t css_uri = { .uri = "/output.css", .method = HTTP_GET, .handler = css_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &css_uri);

    httpd_uri_t ping_uri = { .uri = "/id", .method = HTTP_GET, .handler = ping_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &ping_uri);

    httpd_uri_t js_uri = { .uri = "/script.js", .method = HTTP_GET, .handler = script_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &js_uri);
    
    httpd_uri_t file_upload = { .uri = "/upload/*", .handler = file_upload_handler, .method = HTTP_POST, .user_ctx = server_buffer };
    httpd_register_uri_handler(server, &file_upload);
    
    // httpd_uri_t file_delete = { .uri = "/delete/*", .handler = file_delete_handler, .method = HTTP_POST, .user_ctx = server_buffer };
    // httpd_register_uri_handler(server, &file_delete); // Tạm bỏ, vì hàm delete_selected() sẽ POST đến /delete với POST data, không phải URI

    httpd_uri_t file_edit = { .uri = "/edit/*", .handler = edit_handler, .method = HTTP_GET, .user_ctx = server_buffer };
    httpd_register_uri_handler(server, &file_edit);

    httpd_uri_t file_save = { .uri = "/save/*", .handler = save_handler, .method = HTTP_POST, .user_ctx = server_buffer };
    httpd_register_uri_handler(server, &file_save);

    httpd_uri_t mode_switch = { .uri = "/mode", .handler = mode_switch_handler, .method = HTTP_POST, .user_ctx = server_buffer };
    httpd_register_uri_handler(server, &mode_switch);

    httpd_uri_t remount_uri = { .uri = "/remount", .method = HTTP_POST, .handler = remount_handler, .user_ctx = server_buffer };
    httpd_register_uri_handler(server, &remount_uri);

    // Register a generic /delete POST handler for bulk deletion
    httpd_uri_t bulk_delete = { .uri = "/delete", .handler = file_delete_handler, .method = HTTP_POST, .user_ctx = server_buffer };
    httpd_register_uri_handler(server, &bulk_delete);

    
    httpd_uri_t rename_uri = { .uri = "/rename", .method = HTTP_GET, .handler = rename_handler, .user_ctx = server_buffer };
    httpd_register_uri_handler(server, &rename_uri);

    // Route "tham lam" nhất vẫn được đăng ký CUỐI CÙNG
    httpd_uri_t file_download = { .uri = "/*", .handler = download_handler, .method = HTTP_GET, .user_ctx = server_buffer };
    httpd_register_uri_handler(server, &file_download);

    return ESP_OK;
}