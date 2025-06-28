package wsocket

import (
	"log"
	"net/http"
	"sync"

	"github.com/gorilla/websocket"
)

// for decodeing json message
type DecodedMsg struct {
    Key string `json:"key"`
    Value string `json:"value"`
}

type JSON_List struct {
    Port []string `json:"ports"`
}

// Define callback types for different events
type MessageReceivedCallback func(message string)
type ClientConnectedCallback func()
type ClientDisconnectedCallback func()

func (s *WebSocketServer) triggerClientConnected() {
	for _, callback := range s.clientConnectedEvents {
		callback()
	}
}
func (s *WebSocketServer) triggerMessageReceived(message string) {
	for _, callback := range s.messageReceivedEvents {
		callback(message)
	}
}

func (s *WebSocketServer) triggerClientDisconnected() {
    for _, callback := range s.clientDisconnectedEvents {
        callback()
    }
}

func (s *WebSocketServer) RegisterClientConnectedCallback(callback ClientConnectedCallback) {
	s.clientConnectedEvents = append(s.clientConnectedEvents, callback)
}

func (s *WebSocketServer) RegisterMessageReceivedCallback(callback MessageReceivedCallback) {
	s.messageReceivedEvents = append(s.messageReceivedEvents, callback)
}
func (s *WebSocketServer) RegisterClientDisconnectedCallback(callback ClientDisconnectedCallback) {
    s.clientDisconnectedEvents = append(s.clientDisconnectedEvents, callback)
}


// WebSocketServer manages WebSocket connections and broadcasting
type WebSocketServer struct {
	clients map[*websocket.Conn]bool // Map of connected clients
	mutex   sync.Mutex               // Protect access to the clients map
	upgrader websocket.Upgrader      // Upgrader to handle WebSocket connections

    messageReceivedEvents []MessageReceivedCallback
	clientConnectedEvents []ClientConnectedCallback
	clientDisconnectedEvents []ClientDisconnectedCallback
}

// NewWebSocketServer creates and initializes a new WebSocketServer
func NewWebSocketServer() *WebSocketServer {
	return &WebSocketServer{
		clients: make(map[*websocket.Conn]bool),
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool {
				// Allow all origins for simplicity
				return true
			},
		},
        messageReceivedEvents: []MessageReceivedCallback{},
        clientConnectedEvents: []ClientConnectedCallback{},
		clientDisconnectedEvents: []ClientDisconnectedCallback{},
	}
}

// HandleConnection upgrades HTTP requests to WebSocket and manages the connection
func (s *WebSocketServer) HandleConnection(w http.ResponseWriter, r *http.Request) {
	// Upgrade HTTP connection to WebSocket
	conn, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("Error upgrading connection: %v", err)
		return
	}
    log.Println("new ws client")
	defer conn.Close()

	// Add client to the map
	s.addClient(conn)

	defer s.removeClient(conn)

    s.triggerClientConnected()

	// Handle client messages
	for {
		_, msg, err := conn.ReadMessage()
		if err != nil {
			log.Printf("Error reading message: %v", err)
			break
		}
        s.triggerMessageReceived(string(msg))
	}
}

// Broadcast sends a message to all connected clients
func (s *WebSocketServer) Broadcast(message []byte) {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	for client := range s.clients {
		err := client.WriteMessage(websocket.TextMessage, message)
		if err != nil {
			log.Printf("Error writing to client: %v", err)
			client.Close()
			delete(s.clients, client) // Remove faulty client
		}
	}
}

// addClient adds a WebSocket connection to the server's client map
func (s *WebSocketServer) addClient(conn *websocket.Conn) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	s.clients[conn] = true
}

// removeClient removes a WebSocket connection from the server's client map
func (s *WebSocketServer) removeClient(conn *websocket.Conn) {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	delete(s.clients, conn)

    // this should be here, we want to trigger it after client is remove from list completely
    s.triggerClientDisconnected()
}

func (s *WebSocketServer) CountClients() int {
    return len(s.clients)
}

func (s *WebSocketServer) Send(msg []byte) {
    s.mutex.Lock()
    defer s.mutex.Unlock()
    for client := range s.clients {
        err := client.WriteMessage(websocket.TextMessage, msg)
        if err != nil {
            log.Println("Error sending message to WebSocket client:", err)
            client.Close()
            delete(s.clients, client)
        }
    }
}
