package uart

import (
	"app/internal/utils/setting"
	"encoding/json"
	"fmt"
	"log"
	"time"

	"go.bug.st/serial"
)

var Port serial.Port

func get_available_port() ([]string) {
    ports, err := serial.GetPortsList()
    if err != nil {
        log.Println(err)
        return nil
    }
    if len(ports) == 0 {
        return nil
    }
    return ports
}

func Get_available_port_json() []byte {
    ports := get_available_port()

    if len(ports)==0 {
        ports = append(ports, "None")
    }

    msg := map[string][]string{
        "ports": ports,
    }

    data, err := json.Marshal(msg)
    if err != nil { log.Printf("JSON encode error %s\n", err)}

    return data
}

var mode = &serial.Mode{
    BaudRate: 115200,
    Parity: serial.EvenParity,
    DataBits: 8,
    StopBits: serial.OneStopBit,
}


func Write_device_settings(str string) error {
    var deviceSetting setting.Device_Setting

    err := json.Unmarshal([]byte(str), &deviceSetting)
    if err!=nil{log.Printf("JSON decode error%s\n", err)}

    Port, err := serial.Open(deviceSetting.Write_port, mode)

    if err != nil { 
        log.Printf("Failed to open %s\n", deviceSetting.Write_port)
        return err
    }

    defer Port.Close()


    ssid := fmt.Sprintf("ssid:%s", deviceSetting.SSID)
    pwd := fmt.Sprintf("pwd:%s", deviceSetting.PWD)
    id := fmt.Sprintf("id:%s", deviceSetting.ID)

    log.Println(ssid, pwd, id)

    _, err = Port.Write([]byte(ssid))
    time.Sleep(100 * time.Millisecond)
    _, err = Port.Write([]byte(pwd))
    time.Sleep(100 * time.Millisecond)
    _, err = Port.Write([]byte(id))

    log.Println("Write done")
    return err
}

