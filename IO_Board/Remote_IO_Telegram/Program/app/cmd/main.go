package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"slices"
	"strconv"
	"strings"
	"time"

	"app/internal/api/mqtt_client"
	"app/internal/api/restful"
	"app/internal/api/routes"
	telegrambot "app/internal/api/telegram_bot"
	"app/internal/api/uart"
	"app/internal/api/wsocket"
	"app/internal/service/mdns"
	"app/internal/utils"
	"app/internal/utils/setting"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	"gopkg.in/telebot.v4"
)

var device_name_list = []string{"#"}
var app_name string
var settings *setting.App_Settings
var ws = wsocket.NewWebSocketServer()

var temp_device_data utils.Temp_Storage

var app_mqtt = mqtt_client.MQTT_Subscriber{
    Broker: "localhost",
    Port: 1883,
    Default_topic: device_name_list,
}

func http_handler_init(){
    http.HandleFunc("/", routes.Wifi_config_handle)
    http.HandleFunc("/dashboard", routes.Dashboard_handle)
    http.HandleFunc("/chatsetting", routes.ChatSetting_handle)
    http.HandleFunc("/ws", ws.HandleConnection)
    http.Handle("/src/", http.StripPrefix("/src/", http.FileServer(http.Dir("./src"))))
}

func mqtt_handler_init(){
    app_mqtt.RegisterOnConnectedEventCallback(
        func() {
            log.Println("Connected to broker.")
            MQTT_Event_2_Telebot(&app_mqtt)
        },
    )
    app_mqtt.RegisterOnMessageReceivedEventCallback(
        func(msg mqtt.Message) {
            log.Println(string(msg.Payload()))
            log.Println(msg.Topic())
            substrs := strings.Split(msg.Topic(), "/")
            if len(substrs) == 2{
                if substrs[1] == "info"{
                    err, pd := utils.Devices_manager.RegisterFromMessage(string(msg.Payload()))
                    if err!=nil{
                        log.Println(err)
                    } else{
                        pb_json, err := pd.Generate_device_info_JSON()
                        if err!=nil{log.Println(err)}
                        ws.Send(pb_json)
                    }
                }
                if substrs[1] == "init"{
                    data := map[string]string{
                        "input_fb": substrs[0]+"_led_"+string(msg.Payload()),
                    }
                    js, _ := json.Marshal(data)

                    temp_device_data.Add(substrs[0], 'i', string(msg.Payload()))

                    ws.Send(js)
                }

                // handle LWT message
                if(substrs[1] == "LWT"){
                    if(string(msg.Payload()) == "off"){
                        // Remove 
                        utils.Devices_manager.RemoveDeviceByID(substrs[0])

                        // remove from temp storage
                        temp_device_data.Remove(substrs[0])

                        log.Println(len(utils.Devices_manager.Devices), len(utils.Devices_manager.Registered))
                        data := map[string]string{
                            "remove_device": substrs[0],
                        }
                        js, err := json.Marshal(data)
                        if err!=nil{ log.Println(err)}
                        ws.Send(js)
                    }
                }
            }
            if len(substrs) == 3{
                if substrs[1] == "out" && substrs[2] == "fb"{
                    data := map[string]string{
                        "output_fb": substrs[0]+"_btn_"+string(msg.Payload()),
                    }
                    js, _ := json.Marshal(data)
                    temp_device_data.Add(substrs[0], 'o', string(msg.Payload()))
                    ws.Send(js)
                }
            }
        },
    )

    app_mqtt.RegisterOnDisconnectedEventCallback(
        func(err error) {
            log.Printf("Lost connect to broker: %s", err.Error())
        },
    )
}

func MQTT_Event_2_Telebot(pclient *mqtt_client.MQTT_Subscriber){
    telegrambot.Send("Chatbot started ⚡")

    pclient.RegisterOnMessageReceivedEventCallback(func(msg mqtt.Message) {
        substrs := strings.Split(msg.Topic(), "/")
        if len(substrs)==2 && substrs[1] == "in"{
            if substrs[1] == "in"{
                data := map[string]string{
                    "input_fb": substrs[0]+"_led_"+string(msg.Payload()),
                }
                js, _ := json.Marshal(data)
                temp_device_data.Add(substrs[0], 'i', string(msg.Payload()))
                ws.Send(js)
            }

            result, pchat := settings.Check_valid_chatbot_ID(substrs[0])
            if result {
                data := strings.Split(string(msg.Payload()), ",")
                alias := settings.Check_Enabled_Input(pchat, data[0])
                if alias!=nil{
                    if data[1] == "0" { data[1] = "⚪"}
                    if data[1] == "1" { data[1] = "🔴"}
                    telegrambot.Send(fmt.Sprintf("🖥️Device %s ⚡ %s %s", substrs[0], alias.Alias, data[1]))

                }
            }
        }
    })

	telegrambot.Bot.Handle("/set", func(c telebot.Context) error {
        substrs := strings.Split(c.Text(), " ")
        if len(substrs) == 3{
            pin, d := settings.FindOutAlias(substrs[1]);
            fmt.Println("PIN", strconv.Itoa(pin))


            if d!=nil{ 
                mqtt_client.Client.Publish(fmt.Sprintf("%s/out", d.ID), 1, false, strconv.Itoa(pin))
                fmt.Println(fmt.Sprintf("%s/out", d.ID))
            } else { 
                telegrambot.Send(fmt.Sprintf("❌ Can't find '%s' alias", substrs[1]))
            }
        } else {
            telegrambot.Send("❌ Invalid syntax. Pls try '/set (alias) value'")
        }
        return nil;
	})

	telegrambot.Bot.Handle("/click", func(c telebot.Context) error {
        substrs := strings.Split(c.Text(), " ")
        if len(substrs) == 2{
            pin, d := settings.FindOutAlias(substrs[1]);
            fmt.Println("PIN", strconv.Itoa(pin))

            if d!=nil{ 
                mqtt_client.Client.Publish(fmt.Sprintf("%s/out", d.ID), 1, false, strconv.Itoa(pin))
                time.Sleep(500 * time.Millisecond)
                mqtt_client.Client.Publish(fmt.Sprintf("%s/out", d.ID), 1, false, strconv.Itoa(pin))

                fmt.Println(fmt.Sprintf("%s/out", d.ID))
            } else { 
                telegrambot.Send(fmt.Sprintf("❌ Can't find '%s' alias", substrs[1]))
            }
        } else {
            telegrambot.Send("❌ Invalid syntax. Pls try '/set (alias) value'")
        }
        return nil;
	})

	telegrambot.Bot.Handle("/info", func(c telebot.Context) error {
        info := "Connected Devices :\n"
        if len(utils.Devices_manager.Registered) == 0 {
            info += "¯\\_(ツ)_/¯"
        }

        for _, device := range utils.Devices_manager.Registered{
            info += fmt.Sprintf("🔌 %s\n", device)
        }

        telegrambot.Send(info)
        return nil;
	})
}


func ws_handler_init(){
    ws.RegisterClientConnectedCallback(
        func() {
            log.Printf("New client connected, %d in total\n", ws.CountClients())
        },
    )
    ws.RegisterMessageReceivedCallback(
        func(message string) {
            log.Println(message)

            var jsonData wsocket.DecodedMsg
            err := json.Unmarshal([]byte(message), &jsonData)
            if err != nil {
                log.Printf("WS message error %s\n", err)
            }

            // check command
            if jsonData.Key == "get" {
                if jsonData.Value == "wifi_info" {
                    jsonData, err := json.Marshal(*settings)
                    if err != nil { log.Printf("JSON encode error %s\n", err)}
                    ws.Send([]byte(jsonData))
                }
                if jsonData.Value == "ports" {
                    ws.Send(uart.Get_available_port_json())
                }
                if jsonData.Value == "chat_profile" {
                    jsonData, err := settings.Get_Chat_All_Profile()
                    if err != nil { log.Printf("JSON encode error %s\n", err)}
                    ws.Send([]byte(jsonData))
                }
                if jsonData.Value == "devices_state"{
                    for _, d := range temp_device_data.Devices {
                        if slices.Contains(utils.Devices_manager.Registered, d.ID){
                            mqtt_client.Client.Publish(d.ID + "/cmd", 0, false, []byte("all") );
                        }
                    }
                }
            } else if jsonData.Key == "set" {
                uart.Write_device_settings(jsonData.Value)
            } else if jsonData.Key == "btn_click"{
                strs := strings.Split(jsonData.Value, "_")
                fmt.Println(strs)
                
                if len(strs)==3{
                    mqtt_client.Client.Publish(strs[0] + "/out", 1, false, strs[2]);
                }
            } else if jsonData.Key == "chat_data_update"{
                var chat setting.Device

                err := json.Unmarshal([]byte(jsonData.Value), &chat)
                if err!=nil{
                    log.Printf("Json decode error: %s\n", err)
                    return
                }

                for i, old_chat := range settings.Chat_data {
                    if old_chat.ID == chat.ID{
                        settings.Chat_data[i] = chat
                    }
                }

                setting.SaveSettings("./settings.json", settings)

                jsonData, err := settings.Get_Chat_All_Profile()
                if err != nil { log.Printf("JSON encode error %s\n", err)}
                ws.Send([]byte(jsonData))

                log.Println("Save settings ", settings)

            } else if jsonData.Key == "chat_data_add"{
                var chat setting.Device
                err := json.Unmarshal([]byte(jsonData.Value), &chat)

                if err!=nil{
                    log.Printf("Json decode error:%s \n", err)
                }

                settings.Chat_data = append(settings.Chat_data, chat)
                setting.SaveSettings("settings.json", settings)

                // update front-end data
                jsonData, err := settings.Get_Chat_All_Profile()
                if err != nil { log.Printf("JSON encode error %s\n", err)}
                ws.Send([]byte(jsonData))

                log.Println("Save settings ", settings)
            } else if jsonData.Key == "chat_data_remove"{
                var chat setting.Device
                err := json.Unmarshal([]byte(jsonData.Value), &chat)

                if err!=nil{
                    log.Printf("Json decode error:%s \n", err)
                }

                for i, old_chat := range settings.Chat_data {
                    if old_chat.ID == chat.ID{
                        settings.Remove_device(i)
                    }
                }

                setting.SaveSettings("settings.json", settings)

                // update front-end data
                jsonData, err := settings.Get_Chat_All_Profile()
                if err != nil { log.Printf("JSON encode error %s\n", err)}
                ws.Send([]byte(jsonData))

                log.Println("Save settings ", settings)
            }
        },
    )
}

func main(){
    var err error
    settings, err = setting.LoadSettings("settings.json")
    if err!=nil{ log.Fatal(err) }

    // start telegram bot
    telegrambot.Telebot_Init(settings.Telebot)

    // Start mosquitto service
    // service.Run_mosquitto()
    // log.Println("Mosquitto Started")

    // Start mDNS
    fmt.Println("Bruh")
    go mdns.MDNS_init(&settings.MDNS)
    fmt.Println("Bruh")
    
    // Start MQTT client
    if len(app_mqtt.Client_id) <= 0{
        app_mqtt.Client_id = settings.AppName
    }

    // Start handler
    restful.Restful_api_init()
    http_handler_init()
    ws_handler_init()
    mqtt_handler_init()
    app_mqtt.Connect()

    log.Printf("Start server at :%d\n", settings.HTTP_Port)
    if err := http.ListenAndServe(fmt.Sprintf(":%d", settings.HTTP_Port), nil); err != nil {
        log.Fatal(err);
    }
}
