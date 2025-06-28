class WebSocketClient {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.reconnectDelay = 3000; // Delay before attempting to reconnect
        // this.connect();

        this.eventListeners = new Map();
    }

    addEventListener(eventName, callback) {
        if (!this.eventListeners.has(eventName)) {
            this.eventListeners.set(eventName, []);
        }
        this.eventListeners.get(eventName).push(callback);
    }

    // Dispatch custom event
    dispatchEvent(eventName, detail) {
        if (this.eventListeners.has(eventName)) {
            const eventCallbacks = this.eventListeners.get(eventName);
            eventCallbacks.forEach(callback => callback(detail)); // Pass the event as-is
        }
    }

    connect() {
        this.ws = new WebSocket(this.url);

        this.ws.onopen = () => {
            console.log("Connected to server");
            this.dispatchEvent('onconnected')
        };

        this.ws.onmessage = (event) => {
            this.dispatchEvent('onmessage', event)
        };

        this.ws.onclose = (event) => {
            console.log("Disconnected:", event.reason);
            setTimeout(() => {
                console.log("Reconnecting...");
                this.connect();
            }, this.reconnectDelay);
        };

        this.ws.onerror = (error) => {
            console.error("WebSocket error:", error);
        };
    }

    send(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(data);
        } else {
            console.warn("WebSocket is not open, unable to send data");
        }
    }

    close() {
        if (this.ws) {
            this.ws.close();
            console.log("Websocket closed")
        }
    }
}
