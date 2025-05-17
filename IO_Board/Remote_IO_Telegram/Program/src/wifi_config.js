addEventListener('load', ()=>{
    wsClient.connect();
});

wsClient.addEventListener('onconnected', (_) => {
    wsClient.send(JSON.stringify({
        key: "get",
        value: "wifi_info",
    }));
})

wsClient.addEventListener('onmessage', (event) => {
    try {
        const data = JSON.parse(event.data);
        console.log('Received message:', data);

        if (data.wifi){
            document.getElementById("wifi_ssid").value = data.wifi.ssid;
            document.getElementById("wifi_password").value = data.wifi.password;
        }

        if (data.ports){
            var select = document.getElementById("ports_menu_dropdown");
            select.innerHTML = "";
            console.log(data.ports[0])
            if(data.ports[0] != "None"){
                data.ports.forEach(port => {
                    const option = document.createElement("option");
                    option.value = port;
                    option.textContent = port;
                    select.appendChild(option);
                });
            }
        }

        if (Object.keys(data).length === 0) {
            console.log("No new data received. Skipping update.");
            return;
        }

    } catch (error) {
        console.error("Error processing WebSocket message:", error);
    }
});


document.getElementById("load_port_btn").addEventListener('click', 
    function(){
        wsClient.send(JSON.stringify({
            key: "get",
            value: "ports",
        }));
    }
)

document.getElementById('device_id_input').addEventListener('keydown', function (e) {
    const allowed = /^[a-zA-Z0-9]$/;

    // Allow control keys like backspace, arrows, tab, etc.
    if (
        e.ctrlKey || e.metaKey || e.altKey ||
        ['Backspace', 'ArrowLeft', 'ArrowRight', 'Delete', 'Tab'].includes(e.key)
    ) {
        return;
    }

    // Block if not matching allowed pattern
    if (!allowed.test(e.key)) {
        e.preventDefault();
    }
});

document.getElementById('device_data_form').addEventListener('submit', function (e){
    e.preventDefault()

    var ssid = document.getElementById("wifi_ssid").value;
    var password = document.getElementById("wifi_password").value;
    var id = document.getElementById("device_id_input").value;
    var ports = document.getElementById("ports_menu_dropdown");
    var port = ports.options[ports.selectedIndex].text;

    console.log(ssid);
    console.log(password);
    console.log(id);
    console.log(port);

    var setting = JSON.stringify({
        ssid: ssid,
        pwd:password,
        id:id,
        port:port
    })
    wsClient.send(JSON.stringify({
        key: "set",
        value: setting
    }));
});
