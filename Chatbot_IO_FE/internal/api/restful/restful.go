package restful

import (
	"app/internal/utils"
	"log"
	"net/http"
)
func get_list_devices_handle(w http.ResponseWriter, r *http.Request){
    jData, err := utils.Devices_manager.Generate_all_device_info_JSON()
    if err!=nil{log.Printf("Encode response error: %s\n", err)}

    w.Header().Set("Content-Type", "application/json")
    w.Write(jData)
}
func Restful_api_init(){
    http.HandleFunc("/devices/list/", get_list_devices_handle)
}
