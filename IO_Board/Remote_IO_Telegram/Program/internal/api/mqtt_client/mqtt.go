package mqtt_client

import (
	"fmt"
	"log"
	"time"

	"github.com/eclipse/paho.mqtt.golang"
	"github.com/google/uuid"
)

type MQTT_Subscriber struct {
    Client_id string
    Broker string
    Port uint16
    Default_topic []string

    OnConnectedEvent []OnConnectedEventCallback
    OnDisconnectedLost []OnDisconnectedEventCallback
    OnMessageReceivedEvent []OnMessageReceivedEventCallback
}

func New_uuid() string {
    id := uuid.New()
    return id.String()
} 

var Client mqtt.Client
func (c *MQTT_Subscriber) Connect() {
    opt := mqtt.NewClientOptions()
    opt.AddBroker(fmt.Sprintf("tcp://%s:%d", c.Broker, c.Port))
    opt.SetClientID(c.Client_id)

    // auto connnect
    opt.SetAutoReconnect(true)
    opt.SetConnectRetry(true)

    // ping and keep-alive
    opt.SetKeepAlive(10 * time.Second)
    opt.SetMaxReconnectInterval(100 * time.Millisecond)
    opt.SetPingTimeout(5 * time.Second)


    // last will message
    opt.SetWill("will", fmt.Sprintf("%s disconnected", c.Client_id), 1, false)

    opt.OnConnect = func(_ mqtt.Client) {c.triggerOnConnectedEventCallback()}
    opt.OnConnectionLost = func(_ mqtt.Client, err error) { c.triggerOnDisconnectedCallback(err)}
    opt.SetDefaultPublishHandler(func(_ mqtt.Client, m mqtt.Message) { c.triggerOnMessageReceivedCallback(m)})
    Client = mqtt.NewClient(opt)

    log.Println("Connecting to broker, pls make sure broker is started...")

    log.Println("---------------------------------------------------")
    if token := Client.Connect(); token.Wait() && token.Error() != nil {
        panic(token.Error())
    }

    log.Println("---------------------------------------------------")

    if len(c.Default_topic) > 0 {
        for _, topic := range c.Default_topic{
            token := Client.Subscribe(topic, 1, nil)
            token.Wait()
            log.Printf("sub to %s\n", topic)
        }
    }
}

// event callback for on connected
type OnConnectedEventCallback func()
type OnDisconnectedEventCallback func(err error)
type OnMessageReceivedEventCallback func(msg mqtt.Message)


func (c *MQTT_Subscriber) RegisterOnConnectedEventCallback(callback OnConnectedEventCallback) {
    c.OnConnectedEvent = append(c.OnConnectedEvent, callback)
}

func (c *MQTT_Subscriber) RegisterOnMessageReceivedEventCallback(callback OnMessageReceivedEventCallback) {
    c.OnMessageReceivedEvent = append(c.OnMessageReceivedEvent, callback)
}

func (c *MQTT_Subscriber) RegisterOnDisconnectedEventCallback(callback OnDisconnectedEventCallback){
    c.OnDisconnectedLost = append(c.OnDisconnectedLost, callback)
}


func (c *MQTT_Subscriber) triggerOnMessageReceivedCallback(msg mqtt.Message){
    for _, callback := range c.OnMessageReceivedEvent {
        callback(msg)
    }
}

func (c *MQTT_Subscriber) triggerOnDisconnectedCallback(err error) {
    for _, callback := range c.OnDisconnectedLost {
        callback(err)
    }
}

func (c *MQTT_Subscriber) triggerOnConnectedEventCallback(){
    for _, callback := range c.OnConnectedEvent{
        callback()
    }
}

