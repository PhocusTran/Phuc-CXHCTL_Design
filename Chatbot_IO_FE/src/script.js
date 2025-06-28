const wsClient = new WebSocketClient("ws://localhost:6969/ws");

function flip(i){
    if (i==0) return 1;
    if (i==1) return 0;
}
