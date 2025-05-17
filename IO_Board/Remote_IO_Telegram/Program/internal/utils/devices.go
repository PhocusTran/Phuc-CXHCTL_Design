package utils

import (
	"encoding/json"
	"fmt"
	"log"
	"slices"
	"strconv"
	"strings"
	"sync"
)

// received upon device connect and publish its information
type Device_Info struct {
    ID          string
    Inputs      int
    Outputs     int
}

type Device_Info_JSON struct {
    ID      string `json:"id"`
    Inputs  int    `json:"ins"`
    Outputs int    `json:"outs"`
}

type Devices_Manager struct {
    Devices     []Device_Info
    Registered  []string
    mux         sync.Mutex
}


var Devices_manager Devices_Manager = Devices_Manager{ }


func (d *Device_Info) Generate_device_info_JSON() ([]byte, error){
    payload := map[string]Device_Info_JSON{
        "device_create": {
            ID:      d.ID,
            Inputs:  d.Inputs,
            Outputs: d.Outputs,
        },
    }

    return json.Marshal(payload)
}


func (dm *Devices_Manager) Generate_all_device_info_JSON() ([]byte, error){
    return json.Marshal(dm.Devices)
}

func (dm *Devices_Manager) RemoveDeviceByID(id string) {
    dm.mux.Lock()
    defer dm.mux.Unlock()

    dm.Devices = slices.DeleteFunc(dm.Devices, func(d Device_Info) bool {
        return d.ID == id
    })

    dm.Registered = slices.DeleteFunc(dm.Registered, func(rid string) bool {
        return rid == id
    })
}

func (dm *Devices_Manager) RegisterFromMessage(msg string) (error, *Device_Info) {
    parts := strings.Split(msg, ".")
    if len(parts) != 3 {
        return fmt.Errorf("Invalid format: %s", msg), nil
    }

    if slices.Contains(dm.Registered, parts[0]){
        return fmt.Errorf("Discard duplicated device %s", parts[0]), nil
    }


    ins, err1 := strconv.Atoi(parts[1])
    outs, err2 := strconv.Atoi(parts[2])
    if err1 != nil || err2 != nil {
        return fmt.Errorf("failed to parse inputs/outputs from message: %s", msg), nil
    }

    device := Device_Info{
        ID:      parts[0],
        Inputs:  ins,
        Outputs: outs,
    }

    log.Printf("%s device has been registered.\n", device.ID)

    dm.mux.Lock()
    defer dm.mux.Unlock()

    dm.Registered = append(dm.Registered, parts[0])
    dm.Devices = append(dm.Devices, device)
    return nil, &device
}


