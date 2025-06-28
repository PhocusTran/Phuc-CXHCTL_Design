var profiles;

addEventListener('load', ()=>{
    wsClient.connect();
});

wsClient.addEventListener('onconnected', (_) => {
    console.log("Ws connected");
    wsClient.send(JSON.stringify({
        key: "get",
        value: "chat_profile",
    }));
});

// Alias Table
function addRow(type_select, pin, alias) {
    tableBody = null;
    if(type_select == 1){
        tableBody = document.querySelector("#input_table tbody");
    } else {
        tableBody = document.querySelector("#output_table tbody");
    }


    const row = document.createElement("tr");
    
    const col1 = document.createElement("td");
    col1.className = "border px-4 py-2";
    col1.innerHTML = `<input type="text" value=${pin} onkeypress="return /[0-9]/i.test(event.key)" placeholder="Pin number" class="w-full border border-gray-300 rounded px-2 py-1"/>`;

    const col2 = document.createElement("td");
    col2.className = "border px-4 py-2";
    col2.innerHTML = `<input type="text" placeholder="Value 2" value="${alias}" class="w-full border border-gray-300 rounded px-2 py-1">`;

    row.appendChild(col1);
    row.appendChild(col2);

    tableBody.appendChild(row);
}

function resetTables(){
    document.querySelector(`#input_table tbody`).innerHTML = "";
    document.querySelector(`#output_table tbody`).innerHTML = "";
}

wsClient.addEventListener('onmessage', (event) => {
    try {
        const data = JSON.parse(event.data);

        console.log('Received message:', data);

        profiles = data.chat_data;

        if (data.chat_data){

            var select = document.getElementById("profile_menu_dropdown");
            resetTables();

            select.innerHTML = "";
            if(data.chat_data[0]){
                data.chat_data.forEach(c => {
                    if (c.ID){
                        const option = document.createElement("option");
                        option.text = c.ID;
                        option.value = JSON.stringify(c);
                        select.appendChild(option);
                    };
                });
                var chat = data.chat_data[0];

                chat.Output_enabled.forEach(output => {
                    addRow(0, output.Pin, output.Alias);
                });

                chat.Input_enabled.forEach(output => {
                        addRow(1, output.Pin, output.Alias);
                });

                // document.getElementById("input_notify_enable").value = data.chat_data[0].Input_enabled.join(',');
                // document.getElementById("output_notify_enable").value = data.chat_data[0].Output_enabled.join(',');
                document.getElementById("device_id_input").value = data.chat_data[0].ID;

            }
        }
    } catch (error) {
        console.error("Error processing WebSocket message:", error);
    }
});


function addNewRow(){
    addRow(1, 0, "");
}
function removeRow(is_input) {
    if(is_input){
        which_table = "input_table";
    } else {
        which_table = "output_table";
    }

    const table = document.querySelector(`#${which_table} tbody`);
    if (table.rows.length > 0) {
        table.deleteRow(-1);
    }
}


const dropdown = document.getElementById('profile_menu_dropdown');

dropdown.addEventListener('change', () => {
    const selectedOption = dropdown.options[dropdown.selectedIndex];
    chat = JSON.parse(selectedOption.value);

    resetTables();

    chat.Output_enabled.forEach(output => {
        addRow(0, output.Pin, output.Alias);
    });

    chat.Input_enabled.forEach(output => {
        addRow(1, output.Pin, output.Alias);
    });

    document.getElementById("device_id_input").value = chat.ID;
});

function getFormattedTableData(which_table) {
    const tableBody = document.querySelector(`#${which_table} tbody`);
    const rows = Array.from(tableBody.rows);
    const result = [];

    rows.forEach(row => {
        const inputs = row.querySelectorAll("input");
        if (inputs.length >= 2) {
            const pin = parseInt(inputs[0].value, 10);
            const alias = inputs[1].value.trim();

            if (!isNaN(pin) && alias !== "") {
                result.push({
                    Pin: pin,
                    Alias: alias
                });
            }
        }
    });

    return result;
}


function change_chat_setting(e){
    key = null;

    output_table = `${JSON.stringify(getFormattedTableData("output_table"))}`;
    input_table = `${JSON.stringify(getFormattedTableData("input_table"))}`;

    // console.log(output_table);
    // console.log(input_table);

    const selectedOption = dropdown.options[dropdown.selectedIndex];

    id = document.getElementById("device_id_input").value;


    if(e.target.id == "submit_device_data_btn"){

        // check if ID differnce, if so remove then add new one
        selectedOption.value;
        value = JSON.parse(selectedOption.value);
        if(id != value.ID){
            console.log("id differ");
            key = "chat_data_remove";

            wsClient.send(JSON.stringify({
                key: key,
                value:`{"ID": "${value.ID}", "Input_enabled": ${input_table}, "Output_enabled": ${output_table}}`
            }));
            key = "chat_data_add";

        } else{
            key = "chat_data_update";
        }


    } else if (e.target.id == "add_chat_data_btn"){
        key = "chat_data_add";
    }
    else if (e.target.id == "remove_chat_data_btn"){

        key = "chat_data_remove";
    }


    wsClient.send(JSON.stringify({
        key: key,
        value: `{"ID": "${id}", "Input_enabled": ${input_table}, "Output_enabled": ${output_table}}`
    }));
};

document.getElementById('remove_chat_data_btn').addEventListener('click', change_chat_setting)
document.getElementById('submit_device_data_btn').addEventListener('click', change_chat_setting)
document.getElementById('add_chat_data_btn').addEventListener('click', change_chat_setting)

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
