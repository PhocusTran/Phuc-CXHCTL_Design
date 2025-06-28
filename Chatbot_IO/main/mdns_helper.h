
#ifndef MDNS_HELPER_H
#define MDNS_HELPER_H

#include "mdns.h"
void mdns_server_init();
void query_mdns_host(const char *host_name);
void query_mdns_service(const char *service_name, const char *proto);
mdns_result_t* query_mdns_mqtt_broker(const char *service_name, const char *proto);

static const char* mDNS_hostname = "my_esp32c6";
static const char* mDNS_instance_name = "ESP32-C6";


#endif /* end of include guard: MDNS_HELPER_H */
