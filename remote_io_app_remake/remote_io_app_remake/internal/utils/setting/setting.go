package setting

import (
	"app/internal/service/mdns"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"slices"
	"strconv"
)

// Wifi
type Wifi_Settings struct {
    SSID     string `json:"ssid"`
    Password string `json:"password"`
}

// telegram chat
type Telegram_Bot struct{
    Token   string `json:"token"`
    Chat    string `json:"chat"`
}

// general settings
type App_Settings struct {
    AppName         string              `json:"app_name"`
    Namespace       string              `json:"namespace"`
    HTTP_Port       int                 `json:"http_port"`
    Wifi            Wifi_Settings       `json:"wifi"`
    MDNS            mdns.MDNS_Settings  `json:"mdns"`
    Chatbot_enable  string              `json:"chatbot_enable"`
    Telebot         Telegram_Bot        `json:"telebot"`
    Chat_data       []Device            `json:"chatbot"`
}

// crucial info for device to establish connection with MQTT broker
type Device_Setting struct{
    SSID        string              `json:"ssid"`
    Write_port  string              `json:"port"`
    PWD         string              `json:"pwd"`
    ID          string              `json:"id"`
}

type Device struct {
	ID              string   `json:"ID"`
	InputEnabled    []Alias  `json:"Input_enabled"`
	OutputEnabled   []Alias  `json:"Output_enabled"`
}

type Alias struct {
    Pin         int     `json:"Pin"`
    Alias       string  `json:"Alias"`
}



// SaveSettings saves settings to a JSON file
func SaveSettings(filename string, settings *App_Settings) error {
    data, err := json.MarshalIndent(settings, "", "  ")
    if err != nil {
        return fmt.Errorf("failed to marshal settings: %v", err)
    }
    log.Println(settings)
    return os.WriteFile(filename, data, 0644)
}

// LoadSettings loads settings from a JSON file
func LoadSettings(filename string) (*App_Settings, error) {
    file, err := os.ReadFile(filename)
    if err != nil {
        return nil, fmt.Errorf("failed to read file: %v", err)
    }

    var settings App_Settings
    if err := json.Unmarshal(file, &settings); err != nil {
        return nil, fmt.Errorf("failed to unmarshal JSON: %v", err)
    }

    fmt.Println(settings)

    return &settings, nil
}
func (s *App_Settings)Remove_device(i int){
    s.Chat_data = slices.Delete(s.Chat_data,i, i+1)
}

func (s *App_Settings) Check_valid_chatbot_ID(id string) (bool, *Device) {
    for _, chat := range s.Chat_data{
        if id == chat.ID{
            return true, &chat
        }
    }
    return false, nil
}

func (s *App_Settings) Check_Enabled_Input(device *Device, pin string) *Alias {
    for _, d := range device.InputEnabled {
        if pin == strconv.Itoa(d.Pin){
            return &d
        }
    }
    return nil
}

func (s *App_Settings) Get_Chat_All_Profile() ([]byte, error) {
    raw := map[string][]Device{
        "chat_data": s.Chat_data,
    }
    jsonData, err := json.Marshal(raw)
    if err!=nil{return nil, err}

    return jsonData, nil
}

func (s *App_Settings)FindOutAlias(alias string) (int, *Device) {
    for _, d := range s.Chat_data {
        for _, out := range d.OutputEnabled {
            if out.Alias == alias {
                return out.Pin,&d
            }
        }
    }
    return 0, nil
}

