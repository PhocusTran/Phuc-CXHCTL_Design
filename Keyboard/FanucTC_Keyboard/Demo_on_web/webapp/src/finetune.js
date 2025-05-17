const tableBody = document.querySelector("#dynamicTable tbody");
const addRowButton = document.getElementById("addRowBtn");

function requestImage() {
    socket.emit('request_image', { id: 'result1'});
}

socket.on("table/add_row", (data) => {
    console.log(data);
    const rowHTML = `
        <tr class="border-b border-gray-700" data-id="${data.id}">
            <td class="px-3 py-1">${data.id}</td>
            ${data.columns.map((cell, index) => `
                <td class="px-6 py-1">
                    <div contenteditable="true" 
                    class="editable-cell focus:outline-none focus:ring-2 focus:ring-blue-500">
                    ${cell}
                    </div>
                </td>`).join("")}
            <td class="px-6 py-1">
                <div class="flex justify-center items-center">
                    <input id="checkbox-${data.id}" type="checkbox" value="${data.id}" 
                    class="w-4 h-4 text-green-600 bg-green-400 border-gray-300 rounded" 
                    onclick="toggleHighlight(this)">
                </div>
            </td>
        </tr>`;
    tableBody.innerHTML += rowHTML;
});

// Listen for the event that contains the updated value for the fifth column
socket.on("table/modify_fifth_cell", (updatedData) => {
    const row = document.querySelector(`tr[data-id="${updatedData.id}"]`);
    
    if (row) {
        // Find the fifth column (index 4) and update its value
        fifthColumn = row.querySelectorAll("td")[4];
        thirdColumn = row.querySelectorAll("td")[2];
        sixthColumn = row.querySelectorAll("td")[5];

        fifthColumn.querySelector("div").innerText = updatedData.new_value;

        fifthColumn_value = parseFloat(fifthColumn.querySelector("div").innerText);
        thirdColumn_value = parseFloat(thirdColumn.querySelector("div").innerText);

        sixthColumn.querySelector("div").innerText = (fifthColumn_value - thirdColumn_value).toFixed(2);

        console.log(fifthColumn)

        console.log(thirdColumn_value)
        console.log(thirdColumn)

        console.log(sixthColumn.querySelector("div").innerText)
        console.log(sixthColumn)

    }
});

function getHighlightedRowData() {
    // Initialize an empty array to store the data
    let matrix = [];

    // Select all checkboxes that are checked
    const checkedCheckboxes = document.querySelectorAll('input[type="checkbox"]:checked');

    // Loop over each checked checkbox and get its associated row data
    checkedCheckboxes.forEach(checkbox => {
        // Get the parent <tr> of the checked checkbox
        const row = checkbox.closest('tr');
        
        // Extract the data from the row cells (excluding the checkbox column)
        const rowData = Array.from(row.cells).map(cell => {
            // Skip the checkbox column (last column)
            if (cell.querySelector('input[type="checkbox"]')) {
                return null;
            }
            return cell.innerText.trim();
        }).filter(cellData => cellData !== null);  // Filter out null values (checkbox columns)

        // Add the row data to the matrix
        matrix.push(rowData);
    });

    // Return the matrix
    console.log(matrix)
    socket.emit('finetune', matrix)
    return matrix;
}

function toggleHighlight(checkbox) {
    // Function to toggle highlight class
    const row = checkbox.closest("tr"); // Get the parent row of the checkbox
    if (checkbox.checked) {
        row.classList.add("bg-green-100"); // Add highlight class
    } else {
        row.classList.remove("bg-green-100"); // Remove highlight class
    }
}

// Enable/Disable user interaction
socket.on("table/control", (data) => {
    if (data['isEnable'] == '0') {
        tableBody.style.pointerEvents = "none";
    }
    else if (data['isEnable'] == '1') {
        tableBody.style.pointerEvents = "auto";
    }
});

function sendCheckedCheckboxes() {
    // Gather all checked checkboxes
    const checkedIds = [];
    const checkboxes = document.querySelectorAll('input[type="checkbox"]:checked');
    
    // Loop through the checked checkboxes and get their ids
    checkboxes.forEach(checkbox => {
        checkedIds.push(checkbox.value);
    });
    
    // Send the list of checked checkbox IDs to the server
    socket.emit('table/check', { checkedIds: checkedIds });
    console.log(checkedIds)
}

// Add event listeners to all checkboxes
document.querySelectorAll('input[type="checkbox"]').forEach(checkbox => {
    checkbox.addEventListener('click', sendCheckedCheckboxes);
});

// Example toggleHighlight function
function toggleHighlight(checkbox) {
    const row = checkbox.closest("tr");
    console.log("highlight")
    row.classList.toggle("bg-green-200", checkbox.checked);
    sendCheckedCheckboxes();
}

window.onload = function() {
    socket.emit("table/table_requested");
    // socket.emit("table/random_row");
    requestImage();
};

