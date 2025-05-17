package mdns

import (
	"fmt"
	"log"

	"github.com/grandcat/zeroconf"
)

type MDNS_Settings struct{
    Name            string  `json:"name"`
    Service_type    string  `json:"service_type"`
    Port            int     `json:"port"`
    Mqtt_Port       int     `json:"mqtt_port"`
}

func MDNS_init(settings* MDNS_Settings){
    // service, err := zeroconf.Register(
    //     "MQTT Broker",  // service name
    //     "_mqtt._tcp", // service type
    //     "local.",     // domain
    //     8000,         // port
    //     []string{"txtv=1", "mqtt_role=broker", "mqtt_port=1883"}, // service metadata
    //     nil,                            // interface
    // )
    mqtt_port_mdns_txt := fmt.Sprintf("mqtt_port=%d", settings.Mqtt_Port)
    service, err := zeroconf.Register(
        settings.Name,
        settings.Service_type,
        "local.",     // domain
        settings.Port,
        []string{
            "txtv=1", 
            "mqtt_role=broker", 
            mqtt_port_mdns_txt}, // service metadata
        nil,                            // interface
    )
	if err != nil {
		log.Fatal(err)
	}
	defer service.Shutdown()

	log.Printf("Service published, available on port %d", settings.Port)
    select {}
}

