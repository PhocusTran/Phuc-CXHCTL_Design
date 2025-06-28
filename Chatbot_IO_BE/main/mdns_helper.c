#include "mdns_helper.h"
#include "esp_log.h"
#include "esp_netif_ip_addr.h"
#include <string.h>

void mdns_server_init(){
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(mDNS_hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set(mDNS_instance_name));

    ESP_ERROR_CHECK(mdns_service_add("http", "_http", "_tcp", 80, NULL, 0));
    ESP_LOGI("mDNS", "mDNS service started at %s", mDNS_hostname);
}

const char *ip_protocol_str[] = {"V4", "V6", "MAX"};
void mdns_print_results(mdns_result_t *results)
{
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1, t;
    while (r) {
        if (r->esp_netif) {
            printf("%d: Interface: %s, Type: %s, TTL: %" PRIu32 "\n", i++, esp_netif_get_ifkey(r->esp_netif),
                   ip_protocol_str[r->ip_protocol], r->ttl);
        }
        if (r->instance_name) {
            printf("  PTR : %s.%s.%s\n", r->instance_name, r->service_type, r->proto);
        }
        if (r->hostname) {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if (r->txt_count) {
            printf("  TXT : [%zu] ", r->txt_count);
            for (t = 0; t < r->txt_count; t++) {
                printf("%s=%s(%d); ", r->txt[t].key, r->txt[t].value ? r->txt[t].value : "NULL", r->txt_value_len[t]);
            }
            printf("\n");
        }
        a = r->addr;
        while (a) {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
        r = r->next;
    }
}

mdns_result_t* query_mdns_mqtt_broker(const char *service_name, const char *proto)
{
    char* TAG = "MDNS_DISOCVER";
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if (err) {
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return NULL;
    }
    if (!results) {
        ESP_LOGW(TAG, "No results found!");
        return NULL;
    }

    // mdns_print_results(results);

    mdns_result_t *result = results;
    if(result){

        if (result->txt_count) {
            // the format of txt is 0:txtv 1:mqtt_role 2:mqtt_port
            if(!strcmp(result->txt[1].key, "mqtt_role")){
                // check port

                return (void*)result;

                /*
                */
            }
        }
        result = result->next;
    }
    
    mdns_query_results_free(results);
    return NULL;
}

