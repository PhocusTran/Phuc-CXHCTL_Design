addEventListener('load', ()=>{
    wsClient.connect();
});

function generateButtonGrid(type, container, groundId, totalButtons, columns = 4) {
    container.innerHTML = ''; // clear previous content
    for (let i = 0; i < totalButtons; i += columns) {
        const row = document.createElement('span');
        row.className = 'flex gap-2 justify-between';

        for (let j = 0; j < columns && (i + j) < totalButtons; j++) {
            const element = document.createElement(type);
            element.className = 'w-full rounded-sm bg-red-300 px-3 py-1';
            if(type=="button"){
                element.classList.add("hover:text-gray-200");
                element.addEventListener('click', debounce(output_button_click, 50))
            }
            element.textContent = i + j + 1;
            element.setAttribute("id", `${groundId}_${i+j+1}`);
            row.appendChild(element);
        }

        container.appendChild(row);
    }
}

function generateGrid(deviceId, ins, outs){
    // Outer container
    const outerDiv = document.createElement("div");
    outerDiv.className = "h-fit w-fit justify-between rounded-md bg-white p-2";
    outerDiv.setAttribute("id", deviceId);

    // Inner flex container
    const flexInDiv = document.createElement("div");
    flexInDiv.className = "flex h-1/4 justify-between gap-2 text-3xl";
    
    // Inner flex container
    const flexOutDiv = document.createElement("div");
    flexOutDiv.className = "flex h-1/4 justify-between gap-2 text-3xl";

    // Input box
    const inputDiv = document.createElement("div");
    inputDiv.className = "justify-center w-fit w-full flex items-center rounded-sm bg-gray-200 px-2 py-1";
    inputDiv.textContent = "Input";


    const inputGrid = document.createElement("div");
    inputGrid.className = "flex flex-col gap-2";
    inputGrid.setAttribute("id", "inputGrid")
    generateButtonGrid('span', inputGrid,`${deviceId}_led`, ins);

    // Input box
    const outputDiv = document.createElement("div");
    outputDiv.className = "justify-center w-fit w-full flex items-center rounded-sm bg-gray-200 px-2 py-1";
    outputDiv.textContent = "Output";

    const outputGrid = document.createElement("div");
    outputGrid.className = "flex flex-col gap-2";
    outputGrid.setAttribute("id", "outputGrid");
    generateButtonGrid('button', outputGrid,`${deviceId}_btn`, outs);

    const centerbox = document.createElement("div");
    centerbox.className = "mt-2 mb-2 text-3xl rounded-sm border border-gray-200 px-2 py-4 text-center";
    centerbox.textContent = deviceId;

    flexInDiv.appendChild(inputDiv);
    flexInDiv.appendChild(inputGrid);

    flexOutDiv.appendChild(outputDiv)
    flexOutDiv.appendChild(outputGrid);

    outerDiv.appendChild(flexInDiv);
    outerDiv.appendChild(centerbox);
    outerDiv.appendChild(flexOutDiv);

    const thediv = document.getElementById("thegrid");
    thediv.appendChild(outerDiv);
}

wsClient.addEventListener('onmessage', (event) => {
    try {
        const data = JSON.parse(event.data);
        console.log('Received message:', data);

        if (data.device_create){
            generateGrid(data.device_create.id,
                         data.device_create.ins,
                         data.device_create.outs)
        }
        if (data.remove_device){
            const el = document.getElementById(data.remove_device);
            if (el) el.remove();
        }
        if (data.output_fb){
            const [elementId, state] = data.output_fb.split(",");
            const element = document.getElementById(elementId);

            if (element) {
                if (state === "1") {
                    element.classList.remove("bg-red-300");
                    element.classList.add("bg-green-500");
                } else {
                    element.classList.remove("bg-green-500");
                    element.classList.add("bg-red-300");
                }
            }
        }
        if (data.input_fb){
            const [elementId, state] = data.input_fb.split(",");
            const element = document.getElementById(elementId);

            if (element != null) {
                if (state == 1) {
                    element.classList.remove("bg-red-300");
                    element.classList.add("bg-green-500");
                } else {
                    element.classList.remove("bg-green-500");
                    element.classList.add("bg-red-300");
                }
            }

        }
    } catch (error) {
        console.error("Error processing WebSocket message:", error);
    }
});

async function getData() {
    const url = "/devices/list";
    try {
        const response = await fetch(url);
        if (!response.ok) {
            throw new Error(`Response status: ${response.status}`);
        }

        const json = await response.json();
        if (json){
            json.forEach((item) =>{
                // console.log(item.ID);
                // console.log(item.Inputs);
                // console.log(item.Outputs);
                generateGrid(item.ID,
                    item.Inputs,
                    item.Outputs)
            })
        }

        wsClient.send(JSON.stringify({
            key: "get",
            value: "devices_state",
        }));
    } catch (error) {
        console.error(error.message);
    }
}
getData();


function output_button_click(event){
    const button = event.target.closest("button");
    if (!button) return;

    console.log("Button clicked:", button.id);
    wsClient.send(JSON.stringify({
        key: "btn_click",
        value: button.id,
    }));
}


function debounce(fn, delay=100) {
    let timeout;
    return function (...args) {
        if (timeout) clearTimeout(timeout);
        timeout = setTimeout(() => fn.apply(this, args), delay);
    };
}


wsClient.addEventListener('onconnected', (_) => {
    console.log("Ws connected");
});
