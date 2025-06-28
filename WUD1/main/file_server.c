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
#include "main.h"
#include "task.h"
#include "sys/dirent.h"
#include "mount.h"

#include "tusb_msc_storage.h"
#include "extran_event_callback.h"
#include "usb_msc.h"

static const char* TAG = "FILE";
extern uint8_t op_mode;

extern uint8_t restart_usb_after_write;

extern TaskHandle_t remount_task_handler;
extern char hostname[25];

extra_event_callback mode_switch_req_cb = {0};

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

const char *script = "        <script>\r\n            const contextMenu = document.getElementById(\"context-menu\");\r\n            const deleteForm = document.getElementById(\"delete-form\");\r\n\r\n            function showContextMenu(e, item) {\r\n                e.preventDefault();\r\n                const deletePath = item.dataset.path;\r\n                deleteForm.action = deletePath;\r\n\r\n                // Position the menu\r\n                contextMenu.style.left = `${e.pageX}px`;\r\n                contextMenu.style.top = `${e.pageY}px`;\r\n                contextMenu.classList.remove(\"hidden\");\r\n\r\n                // Hide on outside click\r\n                document.addEventListener(\"click\", hideContextMenu);\r\n            }\r\n\r\n            function hideContextMenu(e) {\r\n                if (!contextMenu.contains(e.target)) {\r\n                    contextMenu.classList.add(\"hidden\");\r\n                    document.removeEventListener(\"click\", hideContextMenu);\r\n                }\r\n            }\r\n\r\n            function setpath() {\r\n                const file = document.getElementById(\"newfile\").files[0];\r\n                if (file) {\r\n                    document.getElementById(\"filepath\").value = file.name;\r\n                }\r\n            }\r\n\r\n            function upload() {\r\n                const fileInput = document.getElementById(\"newfile\");\r\n                const filePath = document.getElementById(\"filepath\").value;\r\n                const uploadPath = \"/upload/\" + filePath;\r\n                const file = fileInput.files[0];\r\n\r\n                if (!file) {\r\n                    alert(\"No file selected!\");\r\n                } else if (!filePath) {\r\n                    alert(\"File path on server is not set!\");\r\n                } else if (filePath.endsWith(\'/\')) {\r\n                    alert(\"File name not specified after path!\");\r\n                } else {\r\n                    fileInput.disabled = true;\r\n                    document.getElementById(\"filepath\").disabled = true;\r\n                    document.getElementById(\"upload_btn\").innerText = \"Uploading\";\r\n                    document.getElementById(\"upload_btn\").disabled = true;\r\n\r\n                    const xhttp = new XMLHttpRequest();\r\n                    xhttp.onreadystatechange = function() {\r\n                        if (xhttp.readyState === 4) {\r\n                            if (xhttp.status === 200) {\r\n                                document.open();\r\n                                document.write(xhttp.responseText);\r\n                                document.close();\r\n                            } else if (xhttp.status === 0) {\r\n                                alert(\"Server closed the connection abruptly!\");\r\n                                location.reload();\r\n                            } else {\r\n                                alert(`${xhttp.status} Error!\\n${xhttp.responseText}`);\r\n                                location.reload();\r\n                            }\r\n                        }\r\n                    };\r\n                    xhttp.open(\"POST\", uploadPath, true);\r\n                    xhttp.send(file);\r\n                }\r\n            }\r\n        </script>\r\n";

const char *deletemenu = "<div id=\"context-menu\" class=\"hidden fixed z-50 bg-white border rounded shadow-md w-32 text-sm\"> <form method=\"post\" id=\"delete-form\"> <button type=\"submit\" class=\"w-full text-left px-4 py-2 hover:bg-red-100 text-red-600\">Delete</button> </form> </div>\r\n";

const char *head1 = 
    "<!doctype html>\n<html lang=\"en\">\n    <head>\n        <meta charset=\"UTF-8\" />\n        <title>";

const char *head2 = "</title>\n        <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\" />\n        <meta name=\"description\" content=\"\" />\n        <link rel=\"icon\" href=\"favicon.png\" />\n <link href=\"/output.css\" rel=\"stylesheet\"> \n    </head>\n <body>";

const char *upload =
    "        <div class=\"space-y-3 rounded-md border border-gray-300 p-4 shadow-sm\">\n"
"            <h2 class=\"text-lg font-semibold\">Upload file</h2>\n"
"\n"
"            <input id=\"newfile\" type=\"file\" onchange=\"setpath()\" class=\"block w-full text-sm file:mr-4 file:rounded file:border-0 file:bg-sky-100 file:p-2 file:text-md file:font-semibold hover:file:bg-sky-200\" />\n"
"\n"
"            <div class=\"flex items-center space-x-2\">\n"
"                <input id=\"filepath\" type=\"text\" placeholder=\"Set path on server\" class=\"w-1/2 rounded border border-gray-300 px-3 py-2 focus:ring-2 focus:ring-sky-300 focus:outline-none\" />\n"
"                <button id=\"upload_btn\" type=\"button\" onclick=\"upload()\" class=\"rounded bg-sky-500 px-4 py-2 text-white shadow hover:bg-sky-600\">Upload</button>\n"
"            </div>\n"
"        </div>\n";

/*    "<script>"
    "function setpath(){document.getElementById(\"filepath\").value=document.getElementById(\"newfile\").files[0].name;}"
    "function upload(){"
    "var filePath=document.getElementById(\"filepath\").value;"
    "var upload_path=\"/upload/\"+filePath;"
    "var file=document.getElementById(\"newfile\").files[0];"
    "var MAX_SIZE=200*1024;var MAX_STR=\"200KB\";"
    "if(!file){alert(\"No file selected!\");}"
    "else if(filePath.length==0){alert(\"File path on server is not set!\");}"
    // "else if(filePath.indexOf(' ')>=0){alert(\"File path on server cannot have spaces!\");}"
    "else if(filePath[filePath.length-1]=='/'){alert(\"File name not specified after path!\");}"
    "else{"
    "document.getElementById(\"newfile\").disabled=true;"
    "document.getElementById(\"filepath\").disabled=true;"
    "document.getElementById(\"upload\").disabled=true;"
    "var xhttp=new XMLHttpRequest();"
    "xhttp.onreadystatechange=function(){"
    "if(xhttp.readyState==4){"
    "if(xhttp.status==200){document.open();document.write(xhttp.responseText);document.close();}"
    "else if(xhttp.status==0){alert(\"Server closed the connection abruptly!\");location.reload()}"
    "else{alert(xhttp.status+\" Error!\\n\"+xhttp.responseText);location.reload()}}};"
    "xhttp.open(\"POST\",upload_path,true);xhttp.send(file);}}"
    "</script></div>";
*/


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
        ESP_LOGE(TAG, "Read path, invalid.");
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid path");
        return ESP_FAIL;
    }
    httpd_resp_sendstr_chunk(r, head1);
    httpd_resp_sendstr_chunk(r, hostname);
    httpd_resp_sendstr_chunk(r, head2);
    httpd_resp_sendstr_chunk(r, upload);
    
    httpd_resp_sendstr_chunk(r, "<div id=\"file-list\" class=\"p-5 space-y-2\">");
    while ((entry = readdir(dir)) != NULL) {

        if(entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(r, "<div class=\"file-item w-fit flex items-center space-x-2 rounded bg-yellow-100 p-2 hover:bg-yellow-200\" data-path=\"/delete/");
        } else {
            httpd_resp_sendstr_chunk(r, "<div class=\"file-item w-fit flex items-center space-x-2 rounded bg-gray-100 p-2 hover:bg-gray-200\" data-path=\"/delete/");
        }

        httpd_resp_sendstr_chunk(r, entry->d_name);
        if(entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");

        httpd_resp_sendstr_chunk(r, "\" data-href=\"");
        httpd_resp_sendstr_chunk(r, r->uri);
        httpd_resp_sendstr_chunk(r, entry->d_name);

        if(entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");

        httpd_resp_sendstr_chunk(r, "\" oncontextmenu=\"showContextMenu(event, this)\">");

        if(entry->d_type == DT_DIR){
            httpd_resp_sendstr_chunk(r, "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"h-5 w-5 text-yellow-600\" fill=\"none\" viewBox=\"0 0 24 24\" stroke=\"currentColor\"> <path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M3 7h5l2 2h11v9H3V7z\" /></svg>");
        } else {
            httpd_resp_sendstr_chunk(r, "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"h-5 w-5\" fill=\"none\" viewBox=\"0 0 24 24\" stroke=\"currentColor\"> <path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M7 0H2V16H14V7H7V0Z\" /> </svg>");
        }
        

        httpd_resp_sendstr_chunk(r, "<a href=\"");
        httpd_resp_sendstr_chunk(r, r->uri);
        httpd_resp_sendstr_chunk(r, entry->d_name);
        if(entry->d_type == DT_DIR) httpd_resp_sendstr_chunk(r, "/");
        httpd_resp_sendstr_chunk(r, "\" class=\"text-gray-800 hover:underline\">");
        httpd_resp_sendstr_chunk(r, entry->d_name);
        httpd_resp_sendstr_chunk(r, "</a>");
        httpd_resp_sendstr_chunk(r, "</div>");

    }
    httpd_resp_sendstr_chunk(r, "</div>");

    closedir(dir);
    httpd_resp_sendstr_chunk(r, deletemenu);
    // httpd_resp_sendstr_chunk(r, script);
    httpd_resp_sendstr_chunk(r, "<script src=\"/script.js\"></script>");
    httpd_resp_sendstr_chunk(r, "</body></html>");
    httpd_resp_send_chunk(r, NULL, 0);


    ESP_LOGI(TAG, "Done!!!");
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
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED,"USB is currently used, tried again latter");

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


esp_err_t file_upload_handler(httpd_req_t *r){
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED,"USB is currently used, tried again latter");

    char filepath[MAX_FILE_PATH];
    struct stat file_stat;
    FILE *file = NULL;

    const char* filename = get_path_from_uri(filepath, ((struct file_server_buffer*)r->user_ctx)->base_path, r->uri + strlen("/upload"), MAX_FILE_PATH);

    if(!filename){
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "File name too long!");
        return ESP_FAIL;
    }

    /* fix %20 */
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
    // printf("%zu\n\r", remaining);
    size_t recieved = 0;
    while(remaining > 0){
        http_busy = 1;

        /* terminate HTTP task, yeild sdcard to USB MSC */
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

        /* Recieve file part by part */
        if((recieved = httpd_req_recv(r, buffer, MIN(remaining, FILE_BUFSIZE))) <= 0 ){
            /* if timeout then continue */
            if(recieved == HTTPD_SOCK_ERR_TIMEOUT){
                ESP_LOGE(TAG, "Timeout, retry");
                continue;
            }

            /* if other error, terminate operation */
            fclose(file);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed.");
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "File reception failed.");
            http_busy = 0;
            return ESP_OK;
        }

        /* Hot fix one wierd bug that the HTTP recieved a too large chunk */
        if(recieved > FILE_BUFSIZE){
            ESP_LOGE(TAG, "chunk too large");
            continue;
        }

        /* write content to file */
        // printf("r %zu l %zu\n\r", recieved, remaining);
        if(recieved && (recieved != fwrite(buffer, 1, recieved, file))){
            /* Write fail */
            fclose(file);
            unlink(filepath);

            ESP_LOGE(TAG, "Fail to write file to storage.");
            httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Fail to write file to storage.");
            http_busy = 0;
            return ESP_OK;
        }

        /* printf("%zu \n\r", recieved); */
        remaining -= recieved;
    }

    /* Close file upon completion */
    fclose(file);
    http_busy = 0;
    ESP_LOGI(TAG, "File transfer completed.");

    httpd_resp_set_status(r, "303 See Other");
    httpd_resp_set_hdr(r, "Location", "/");
    httpd_resp_set_hdr(r, "Connection", "close");
    httpd_resp_sendstr(r, NULL);

    // if(restart_usb_after_write) tiny_usb_remount();
    // if(restart_usb_after_write) xTaskNotifyFromISR(remount_task_handler, 1, eSetBits, NULL);
    if(restart_usb_after_write && remount_task_handler != NULL) xTaskNotify(remount_task_handler, 1, eSetBits);

    return ESP_OK;
}

esp_err_t file_delete_handler(httpd_req_t *r){
    if(op_mode == USB_MODE) return usb_mode_block_response(r);
    if(usb_fs_busy) return httpd_resp_send_err(r, HTTPD_405_METHOD_NOT_ALLOWED,"USB is currently used, tried again latter");

    
    char filepath[MAX_FILE_PATH];
    struct stat file_stat;

    const char* filename = get_path_from_uri(filepath, ((struct file_server_buffer *)r->user_ctx)->base_path, r->uri + strlen("/delete"), sizeof(filepath));
    ESP_LOGI(TAG, "Filename %s\n", filename);

    /* fix %20 */
    if(strchr(filepath, '%')){
        char fix_filepath[sizeof(filepath)];
        strncpy(fix_filepath, filepath, sizeof(filepath));
        memset(filepath, 0, sizeof(filepath));
        if(fix_space(filepath, fix_filepath)) strncpy(filepath, fix_filepath, sizeof(filepath));
    }

    if(!filename){
        ESP_LOGE(TAG, "Filename too long!");
        httpd_resp_send_err(r, HTTPD_414_URI_TOO_LONG, "Filename too long.");
    }


    if(stat(filepath, &file_stat) == -1){
        ESP_LOGE(TAG, "File doesn't exsist.");
        httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "File doesn't exsist.");
    }

    if(filename[strlen(filename) - 1] == '/'){ 
        filepath[strlen(filepath)-1] = '\0';
        printf("%s\n\r", filepath);
        ESP_LOGE(TAG, "Dir delete %d.", rmdir(filepath));
        return ESP_OK;
    }
    unlink(filepath);

    ESP_LOGI(TAG, "File deleted.");
    httpd_resp_set_status(r, "303 See Other");
    httpd_resp_set_hdr(r, "Location", "/");
    httpd_resp_set_hdr(r, "Connection", "close");
    // httpd_resp_sendstr(r, "File successfully deleted");
    httpd_resp_sendstr(r, NULL);

    // if(restart_usb_after_write) tiny_usb_remount();
    // if(restart_usb_after_write) xTaskNotify(remount_task_handler, 1, eSetBits);
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
    httpd_resp_set_type(req, "text/css");
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

    // server_buffer = calloc(1, sizeof(struct file_server_buffer));
    // server_buffer = (struct file_server_buffer*)heap_caps_malloc(sizeof(struct file_server_buffer), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    server_buffer = (struct file_server_buffer*)heap_caps_malloc(sizeof(struct file_server_buffer), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    memset(server_buffer, 0, sizeof(struct file_server_buffer));


    if(!server_buffer){
        ESP_LOGE(TAG, "Cannot allocate buffer for file server.");
        return ESP_ERR_NO_MEM;
    }

    strlcpy(server_buffer->base_path, BASE_PATH,
            sizeof(server_buffer->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = 1;
    config.stack_size = 8 * 1024;
    config.keep_alive_enable = true;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if(httpd_start(&server, &config) != ESP_OK){
        ESP_LOGE(TAG, "Failed to start file server...");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Server started");

    /* serve javascript file */
    httpd_uri_t css_uri = {
        .uri       = "/output.css",
        .method    = HTTP_GET,
        .handler   = css_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &css_uri);

    /* serve javascript file */
    httpd_uri_t ping_uri = {
        .uri       = "/id",
        .method    = HTTP_GET,
        .handler   = ping_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &ping_uri);

    /* serve css file */
    httpd_uri_t js_uri = {
        .uri       = "/script.js",
        .method    = HTTP_GET,
        .handler   = script_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &js_uri);

    /* handle root and download file. */
    httpd_uri_t file_download = {
        .uri = "/*",
        .handler = download_handler,
        .method = HTTP_GET,
        .user_ctx = server_buffer,
    };
    httpd_register_uri_handler(server, &file_download);
    
    /* handle file upload. */
    httpd_uri_t file_upload = {
        .uri = "/upload/*",
        .handler = file_upload_handler,
        .method = HTTP_POST,
        .user_ctx = server_buffer,
    };
    httpd_register_uri_handler(server, &file_upload);
    
    /* handle file upload. */
    httpd_uri_t file_delete = {
        .uri = "/delete/*",
        .handler = file_delete_handler,
        .method = HTTP_POST,
        .user_ctx = server_buffer,
    };
    httpd_register_uri_handler(server, &file_delete);

    /* handle file upload. */
    httpd_uri_t mode_switch = {
        .uri = "/mode",
        .handler = mode_switch_handler,
        .method = HTTP_POST,
        .user_ctx = server_buffer,
    };
    httpd_register_uri_handler(server, &mode_switch);

    return ESP_OK;
}
